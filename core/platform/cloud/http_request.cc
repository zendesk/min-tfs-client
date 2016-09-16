/* Copyright 2016 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/core/platform/cloud/http_request.h"

#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/lib/gtl/map_util.h"
#include "tensorflow/core/lib/strings/scanner.h"
#include "tensorflow/core/lib/strings/str_util.h"
#include "tensorflow/core/platform/env.h"
#include "tensorflow/core/platform/protobuf.h"
#include "tensorflow/core/platform/types.h"
#include "tensorflow/core/public/version.h"

namespace tensorflow {

namespace {

// Windows is not currently supported.
constexpr char kCurlLibLinux[] = "libcurl.so.3";
constexpr char kCurlLibMac[] = "/usr/lib/libcurl.3.dylib";

constexpr char kCertsPath[] = "/etc/ssl/certs";

// Set to 1 to enable verbose debug output from curl.
constexpr uint64 kVerboseOutput = 0;

// Timeout for the whole request. Set only to prevent hanging indefinitely.
constexpr uint32 kRequestTimeoutSeconds = 3600;  // 1 hour
// Timeout for the connection phase.
constexpr uint32 kConnectTimeoutSeconds = 120;  // 2 minutes

/// An implementation that dynamically loads libcurl and forwards calls to it.
class LibCurlProxy : public LibCurl {
 public:
  static LibCurlProxy* Load() {
    static LibCurlProxy* libcurl = []() -> LibCurlProxy* {
      std::unique_ptr<LibCurlProxy> libcurl(new LibCurlProxy);
      Status status = libcurl->LoadAndBind();
      if (!status.ok()) {
        return nullptr;
      }
      libcurl->curl_global_init_(CURL_GLOBAL_ALL);
      return libcurl.release();
    }();

    return libcurl;
  }

  CURL* curl_easy_init() override {
    return curl_easy_init_();
  }

  CURLcode curl_easy_setopt(CURL* curl, CURLoption option,
                            uint64 param) override {
    return curl_easy_setopt_(curl, option, param);
  }

  CURLcode curl_easy_setopt(CURL* curl, CURLoption option,
                            const char* param) override {
    return curl_easy_setopt_(curl, option, param);
  }
  CURLcode curl_easy_setopt(CURL* curl, CURLoption option,
                            void* param) override {
    return curl_easy_setopt_(curl, option, param);
  }
  CURLcode curl_easy_setopt(CURL* curl, CURLoption option,
                            size_t (*param)(void*, size_t, size_t,
                                            FILE*)) override {
    return curl_easy_setopt_(curl, option, param);
  }
  CURLcode curl_easy_setopt(CURL* curl, CURLoption option,
                            size_t (*param)(const void*, size_t, size_t,
                                            void*)) override {
    return curl_easy_setopt_(curl, option, param);
  }

  CURLcode curl_easy_perform(CURL* curl) override {
    return curl_easy_perform_(curl);
  }

  CURLcode curl_easy_getinfo(CURL* curl, CURLINFO info,
                             uint64* value) override {
    return curl_easy_getinfo_(curl, info, value);
  }
  CURLcode curl_easy_getinfo(CURL* curl, CURLINFO info,
                             double* value) override {
    return curl_easy_getinfo_(curl, info, value);
  }
  void curl_easy_cleanup(CURL* curl) override {
    return curl_easy_cleanup_(curl);
  }
  char* curl_easy_escape(CURL* curl, const char* str, int length) override {
    return curl_easy_escape_(curl, str, length);
  }

  curl_slist* curl_slist_append(curl_slist* list, const char* str) override {
    return curl_slist_append_(list, str);
  }

  void curl_slist_free_all(curl_slist* list) override {
    return curl_slist_free_all_(list);
  }

  void curl_free(void* p) override {
    curl_free_(p);
  }

 private:
  Status LoadAndBind() {
    auto TryLoadAndBind = [this](const char* name, void** handle) -> Status {
      TF_RETURN_IF_ERROR(Env::Default()->LoadLibrary(name, handle));
#define BIND_CURL_FUNC(function)                             \
  do {                                                       \
    void* symbol_ptr = nullptr;                              \
    TF_RETURN_IF_ERROR(Env::Default()->GetSymbolFromLibrary( \
        *handle, #function, &symbol_ptr));                   \
    *reinterpret_cast<void**>(&(function##_)) = symbol_ptr;  \
  } while (0)

      BIND_CURL_FUNC(curl_global_init);
      BIND_CURL_FUNC(curl_easy_init);
      BIND_CURL_FUNC(curl_easy_setopt);
      BIND_CURL_FUNC(curl_easy_perform);
      BIND_CURL_FUNC(curl_easy_getinfo);
      BIND_CURL_FUNC(curl_slist_append);
      BIND_CURL_FUNC(curl_slist_free_all);
      BIND_CURL_FUNC(curl_easy_cleanup);
      BIND_CURL_FUNC(curl_easy_escape);
      BIND_CURL_FUNC(curl_free);
#undef BIND_CURL_FUNC
      return Status::OK();
    };

    // This may have been linked statically; if curl_easy_init is in the
    // current binary, no need to search for a dynamic version.
    Status status = TryLoadAndBind(nullptr, &handle_);
    if (status.ok()) {
      return status;
    }
    status = TryLoadAndBind(kCurlLibLinux, &handle_);
    if (status.ok()) {
      return status;
    }
    return TryLoadAndBind(kCurlLibMac, &handle_);
  }

  void* handle_ = nullptr;
  CURLcode (*curl_global_init_)(int64) = nullptr;
  CURL* (*curl_easy_init_)(void) = nullptr;
  CURLcode (*curl_easy_setopt_)(CURL*, CURLoption, ...) = nullptr;
  CURLcode (*curl_easy_perform_)(CURL* curl) = nullptr;
  CURLcode (*curl_easy_getinfo_)(CURL* curl, CURLINFO info, ...) = nullptr;
  void (*curl_easy_cleanup_)(CURL* curl) = nullptr;
  curl_slist* (*curl_slist_append_)(curl_slist* list,
                                    const char* str) = nullptr;
  void (*curl_slist_free_all_)(curl_slist* list) = nullptr;
  char* (*curl_easy_escape_)(CURL* curl, const char* str, int length) = nullptr;
  void (*curl_free_)(void* p) = nullptr;
};
}  // namespace

HttpRequest::HttpRequest() : HttpRequest(LibCurlProxy::Load()) {}

HttpRequest::HttpRequest(LibCurl* libcurl)
    : libcurl_(libcurl),
      default_response_buffer_(new char[CURL_MAX_WRITE_SIZE]) {}

HttpRequest::~HttpRequest() {
  if (curl_headers_) {
    libcurl_->curl_slist_free_all(curl_headers_);
  }
  if (put_body_) {
    fclose(put_body_);
  }
  if (curl_) {
    libcurl_->curl_easy_cleanup(curl_);
  }
}

Status HttpRequest::Init() {
  if (is_initialized_) {
    return errors::FailedPrecondition("Already initialized.");
  }
  if (!libcurl_) {
    return errors::FailedPrecondition(
        "Could not initialize the libcurl library. Please make sure that "
        "libcurl is installed in the OS or statically linked to the "
        "TensorFlow binary.");
  }
  curl_ = libcurl_->curl_easy_init();
  if (!curl_) {
    return errors::Internal("Couldn't initialize a curl session.");
  }

  libcurl_->curl_easy_setopt(curl_, CURLOPT_VERBOSE, kVerboseOutput);
  libcurl_->curl_easy_setopt(curl_, CURLOPT_CAPATH, kCertsPath);
  libcurl_->curl_easy_setopt(
      curl_, CURLOPT_USERAGENT,
      strings::StrCat("TensorFlow/", TF_VERSION_STRING).c_str());
  // Do not use signals for timeouts - does not work in multi-threaded programs.
  libcurl_->curl_easy_setopt(curl_, CURLOPT_NOSIGNAL, 1L);
  libcurl_->curl_easy_setopt(curl_, CURLOPT_TIMEOUT, kRequestTimeoutSeconds);
  libcurl_->curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT,
                             kConnectTimeoutSeconds);

  // If response buffer is not set, libcurl will print results to stdout,
  // so we always set it.
  is_initialized_ = true;
  auto s = SetResultBuffer(default_response_buffer_.get(), CURL_MAX_WRITE_SIZE,
                           &default_response_string_piece_);
  if (!s.ok()) {
    is_initialized_ = false;
    return s;
  }
  return Status::OK();
}

string HttpRequest::EscapeString(const string& str) {
  char* out_char_str = libcurl_->curl_easy_escape(curl_, str.c_str(), 0);
  string out_str(out_char_str);
  libcurl_->curl_free(out_char_str);
  return out_str;
}

Status HttpRequest::SetUri(const string& uri) {
  TF_RETURN_IF_ERROR(CheckInitialized());
  TF_RETURN_IF_ERROR(CheckNotSent());
  is_uri_set_ = true;
  libcurl_->curl_easy_setopt(curl_, CURLOPT_URL, uri.c_str());
  return Status::OK();
}

Status HttpRequest::SetRange(uint64 start, uint64 end) {
  TF_RETURN_IF_ERROR(CheckInitialized());
  TF_RETURN_IF_ERROR(CheckNotSent());
  libcurl_->curl_easy_setopt(curl_, CURLOPT_RANGE,
                             strings::StrCat(start, "-", end).c_str());
  return Status::OK();
}

Status HttpRequest::AddHeader(const string& name, const string& value) {
  TF_RETURN_IF_ERROR(CheckInitialized());
  TF_RETURN_IF_ERROR(CheckNotSent());
  curl_headers_ = libcurl_->curl_slist_append(
      curl_headers_, strings::StrCat(name, ": ", value).c_str());
  return Status::OK();
}

Status HttpRequest::AddAuthBearerHeader(const string& auth_token) {
  TF_RETURN_IF_ERROR(CheckInitialized());
  TF_RETURN_IF_ERROR(CheckNotSent());
  if (!auth_token.empty()) {
    return AddHeader("Authorization", strings::StrCat("Bearer ", auth_token));
  }
  return Status::OK();
}

Status HttpRequest::SetDeleteRequest() {
  TF_RETURN_IF_ERROR(CheckInitialized());
  TF_RETURN_IF_ERROR(CheckNotSent());
  TF_RETURN_IF_ERROR(CheckMethodNotSet());
  is_method_set_ = true;
  libcurl_->curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, "DELETE");
  return Status::OK();
}

Status HttpRequest::SetPutFromFile(const string& body_filepath, size_t offset) {
  TF_RETURN_IF_ERROR(CheckInitialized());
  TF_RETURN_IF_ERROR(CheckNotSent());
  TF_RETURN_IF_ERROR(CheckMethodNotSet());
  is_method_set_ = true;
  if (put_body_) {
    fclose(put_body_);
  }
  put_body_ = fopen(body_filepath.c_str(), "r");
  if (!put_body_) {
    return errors::InvalidArgument("Couldn't open the specified file: " +
                                   body_filepath);
  }
  fseek(put_body_, 0, SEEK_END);
  const auto size = ftell(put_body_) - offset;
  fseek(put_body_, offset, SEEK_SET);

  curl_headers_ = libcurl_->curl_slist_append(
      curl_headers_, strings::StrCat("Content-Length: ", size).c_str());
  libcurl_->curl_easy_setopt(curl_, CURLOPT_PUT, 1);
  libcurl_->curl_easy_setopt(curl_, CURLOPT_READDATA,
                             reinterpret_cast<void*>(put_body_));
  return Status::OK();
}

Status HttpRequest::SetPutEmptyBody() {
  TF_RETURN_IF_ERROR(CheckInitialized());
  TF_RETURN_IF_ERROR(CheckNotSent());
  TF_RETURN_IF_ERROR(CheckMethodNotSet());
  is_method_set_ = true;
  libcurl_->curl_easy_setopt(curl_, CURLOPT_PUT, 1);
  curl_headers_ =
      libcurl_->curl_slist_append(curl_headers_, "Content-Length: 0");
  libcurl_->curl_easy_setopt(curl_, CURLOPT_READFUNCTION,
                             &HttpRequest::ReadCallback);
  return Status::OK();
}

Status HttpRequest::SetPostFromBuffer(const char* buffer, size_t size) {
  TF_RETURN_IF_ERROR(CheckInitialized());
  TF_RETURN_IF_ERROR(CheckNotSent());
  TF_RETURN_IF_ERROR(CheckMethodNotSet());
  is_method_set_ = true;
  curl_headers_ = libcurl_->curl_slist_append(
      curl_headers_, strings::StrCat("Content-Length: ", size).c_str());
  libcurl_->curl_easy_setopt(curl_, CURLOPT_POST, 1);
  libcurl_->curl_easy_setopt(curl_, CURLOPT_READDATA,
                             reinterpret_cast<void*>(this));
  libcurl_->curl_easy_setopt(curl_, CURLOPT_READFUNCTION,
                             &HttpRequest::ReadCallback);
  post_body_buffer_ = StringPiece(buffer, size);
  return Status::OK();
}

Status HttpRequest::SetPostEmptyBody() {
  TF_RETURN_IF_ERROR(CheckInitialized());
  TF_RETURN_IF_ERROR(CheckNotSent());
  TF_RETURN_IF_ERROR(CheckMethodNotSet());
  is_method_set_ = true;
  libcurl_->curl_easy_setopt(curl_, CURLOPT_POST, 1);
  curl_headers_ =
      libcurl_->curl_slist_append(curl_headers_, "Content-Length: 0");
  libcurl_->curl_easy_setopt(curl_, CURLOPT_READFUNCTION,
                             &HttpRequest::ReadCallback);
  return Status::OK();
}

Status HttpRequest::SetResultBuffer(char* scratch, size_t size,
                                    StringPiece* result) {
  TF_RETURN_IF_ERROR(CheckInitialized());
  TF_RETURN_IF_ERROR(CheckNotSent());
  if (!scratch) {
    return errors::InvalidArgument("scratch cannot be null");
  }
  if (!result) {
    return errors::InvalidArgument("result cannot be null");
  }
  if (size <= 0) {
    return errors::InvalidArgument("buffer size should be positive");
  }

  response_buffer_ = scratch;
  response_buffer_size_ = size;
  response_string_piece_ = result;
  response_buffer_written_ = 0;

  libcurl_->curl_easy_setopt(curl_, CURLOPT_WRITEDATA,
                             reinterpret_cast<void*>(this));
  libcurl_->curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION,
                             &HttpRequest::WriteCallback);
  return Status::OK();
}

size_t HttpRequest::WriteCallback(const void* ptr, size_t size, size_t nmemb,
                                  void* this_object) {
  CHECK(ptr);
  auto that = reinterpret_cast<HttpRequest*>(this_object);
  CHECK(that->response_buffer_);
  CHECK(that->response_buffer_size_ >= that->response_buffer_written_);
  const size_t bytes_to_copy =
      std::min(size * nmemb,
               that->response_buffer_size_ - that->response_buffer_written_);
  memcpy(that->response_buffer_ + that->response_buffer_written_, ptr,
         bytes_to_copy);
  that->response_buffer_written_ += bytes_to_copy;
  return bytes_to_copy;
}

size_t HttpRequest::ReadCallback(void* ptr, size_t size, size_t nmemb,
                                 FILE* this_object) {
  CHECK(ptr);
  auto that = reinterpret_cast<HttpRequest*>(this_object);
  CHECK(that->post_body_read_ <= that->post_body_buffer_.size());
  const size_t bytes_to_copy = std::min(
      size * nmemb, that->post_body_buffer_.size() - that->post_body_read_);
  memcpy(ptr, that->post_body_buffer_.data() + that->post_body_read_,
         bytes_to_copy);
  that->post_body_read_ += bytes_to_copy;
  return bytes_to_copy;
}

size_t HttpRequest::HeaderCallback(const void* ptr, size_t size, size_t nmemb,
                                   void* this_object) {
  CHECK(ptr);
  auto that = reinterpret_cast<HttpRequest*>(this_object);
  StringPiece header(reinterpret_cast<const char*>(ptr), size * nmemb);
  StringPiece name, value;
  // The supplied header has the form "<name>: <value>", parse it.
  if (strings::Scanner(header)
          .ScanEscapedUntil(':')
          .StopCapture()
          .OneLiteral(": ")
          .GetResult(&value, &name)) {
    string str_value = value.ToString();
    str_util::StripTrailingWhitespace(&str_value);
    that->response_headers_[name.ToString()] = str_value;
  }
  return size * nmemb;
}

Status HttpRequest::Send() {
  TF_RETURN_IF_ERROR(CheckInitialized());
  TF_RETURN_IF_ERROR(CheckNotSent());
  is_sent_ = true;
  if (!is_uri_set_) {
    return errors::FailedPrecondition("URI has not been set.");
  }
  if (curl_headers_) {
    libcurl_->curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, curl_headers_);
  }
  libcurl_->curl_easy_setopt(curl_, CURLOPT_HEADERDATA,
                             reinterpret_cast<void*>(this));
  libcurl_->curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION,
                             &HttpRequest::HeaderCallback);

  char error_buffer[CURL_ERROR_SIZE];
  libcurl_->curl_easy_setopt(curl_, CURLOPT_ERRORBUFFER, error_buffer);

  const auto curl_result = libcurl_->curl_easy_perform(curl_);

  double written_size = 0;
  libcurl_->curl_easy_getinfo(curl_, CURLINFO_SIZE_DOWNLOAD, &written_size);

  libcurl_->curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &response_code_);

  switch (response_code_) {
    case 200:  // OK
    case 201:  // Created
    case 204:  // No Content
    case 206:  // Partial Content
      if (curl_result != CURLE_OK) {
        return errors::Internal(string("curl error: ") + error_buffer);
      }
      if (response_buffer_ && response_string_piece_) {
        *response_string_piece_ = StringPiece(response_buffer_, written_size);
      }
      return Status::OK();
    case 401:
    case 403:
      return errors::PermissionDenied("Not authorized to access the resource.");
    case 404:
      return errors::NotFound("The requested resource was not found.");
    case 416:  // Requested Range Not Satisfiable
      if (response_string_piece_) {
        *response_string_piece_ = StringPiece();
      }
      return Status::OK();
    default:
      return errors::Unavailable(
          strings::StrCat("Unexpected response code ", response_code_));
  }
}

Status HttpRequest::CheckInitialized() const {
  if (!is_initialized_) {
    return errors::FailedPrecondition("The object has not been initialized.");
  }
  return Status::OK();
}

Status HttpRequest::CheckMethodNotSet() const {
  if (is_method_set_) {
    return errors::FailedPrecondition("HTTP method has been already set.");
  }
  return Status::OK();
}

Status HttpRequest::CheckNotSent() const {
  if (is_sent_) {
    return errors::FailedPrecondition("The request has already been sent.");
  }
  return Status::OK();
}

string HttpRequest::GetResponseHeader(const string& name) const {
  const auto& header = response_headers_.find(name);
  return header != response_headers_.end() ? header->second : "";
}

uint64 HttpRequest::GetResponseCode() const { return response_code_; }

}  // namespace tensorflow

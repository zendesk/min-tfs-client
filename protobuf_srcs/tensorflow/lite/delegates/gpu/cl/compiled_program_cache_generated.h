/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

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
// automatically generated by the FlatBuffers compiler, do not modify


#ifndef FLATBUFFERS_GENERATED_COMPILEDPROGRAMCACHE_TFLITE_GPU_CL_DATA_H_
#define FLATBUFFERS_GENERATED_COMPILEDPROGRAMCACHE_TFLITE_GPU_CL_DATA_H_

#include "flatbuffers/flatbuffers.h"

// Ensure the included flatbuffers.h is the same version as when this file was
// generated, otherwise it may not be compatible.
static_assert(FLATBUFFERS_VERSION_MAJOR == 23 &&
              FLATBUFFERS_VERSION_MINOR == 5 &&
              FLATBUFFERS_VERSION_REVISION == 26,
             "Non-compatible flatbuffers version included");

namespace tflite {
namespace gpu {
namespace cl {
namespace data {

struct Program;
struct ProgramBuilder;

struct CompiledCache;
struct CompiledCacheBuilder;

struct Program FLATBUFFERS_FINAL_CLASS : private ::flatbuffers::Table {
  typedef ProgramBuilder Builder;
  enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
    VT_FINGERPRINT = 4,
    VT_BINARY = 6
  };
  uint64_t fingerprint() const {
    return GetField<uint64_t>(VT_FINGERPRINT, 0);
  }
  const ::flatbuffers::Vector<uint8_t> *binary() const {
    return GetPointer<const ::flatbuffers::Vector<uint8_t> *>(VT_BINARY);
  }
  bool Verify(::flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyField<uint64_t>(verifier, VT_FINGERPRINT, 8) &&
           VerifyOffset(verifier, VT_BINARY) &&
           verifier.VerifyVector(binary()) &&
           verifier.EndTable();
  }
};

struct ProgramBuilder {
  typedef Program Table;
  ::flatbuffers::FlatBufferBuilder &fbb_;
  ::flatbuffers::uoffset_t start_;
  void add_fingerprint(uint64_t fingerprint) {
    fbb_.AddElement<uint64_t>(Program::VT_FINGERPRINT, fingerprint, 0);
  }
  void add_binary(::flatbuffers::Offset<::flatbuffers::Vector<uint8_t>> binary) {
    fbb_.AddOffset(Program::VT_BINARY, binary);
  }
  explicit ProgramBuilder(::flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  ::flatbuffers::Offset<Program> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = ::flatbuffers::Offset<Program>(end);
    return o;
  }
};

inline ::flatbuffers::Offset<Program> CreateProgram(
    ::flatbuffers::FlatBufferBuilder &_fbb,
    uint64_t fingerprint = 0,
    ::flatbuffers::Offset<::flatbuffers::Vector<uint8_t>> binary = 0) {
  ProgramBuilder builder_(_fbb);
  builder_.add_fingerprint(fingerprint);
  builder_.add_binary(binary);
  return builder_.Finish();
}

inline ::flatbuffers::Offset<Program> CreateProgramDirect(
    ::flatbuffers::FlatBufferBuilder &_fbb,
    uint64_t fingerprint = 0,
    const std::vector<uint8_t> *binary = nullptr) {
  auto binary__ = binary ? _fbb.CreateVector<uint8_t>(*binary) : 0;
  return tflite::gpu::cl::data::CreateProgram(
      _fbb,
      fingerprint,
      binary__);
}

struct CompiledCache FLATBUFFERS_FINAL_CLASS : private ::flatbuffers::Table {
  typedef CompiledCacheBuilder Builder;
  enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
    VT_DRIVER_VERSION = 4,
    VT_PROGRAMS = 6
  };
  const ::flatbuffers::String *driver_version() const {
    return GetPointer<const ::flatbuffers::String *>(VT_DRIVER_VERSION);
  }
  const ::flatbuffers::Vector<::flatbuffers::Offset<tflite::gpu::cl::data::Program>> *programs() const {
    return GetPointer<const ::flatbuffers::Vector<::flatbuffers::Offset<tflite::gpu::cl::data::Program>> *>(VT_PROGRAMS);
  }
  bool Verify(::flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffset(verifier, VT_DRIVER_VERSION) &&
           verifier.VerifyString(driver_version()) &&
           VerifyOffset(verifier, VT_PROGRAMS) &&
           verifier.VerifyVector(programs()) &&
           verifier.VerifyVectorOfTables(programs()) &&
           verifier.EndTable();
  }
};

struct CompiledCacheBuilder {
  typedef CompiledCache Table;
  ::flatbuffers::FlatBufferBuilder &fbb_;
  ::flatbuffers::uoffset_t start_;
  void add_driver_version(::flatbuffers::Offset<::flatbuffers::String> driver_version) {
    fbb_.AddOffset(CompiledCache::VT_DRIVER_VERSION, driver_version);
  }
  void add_programs(::flatbuffers::Offset<::flatbuffers::Vector<::flatbuffers::Offset<tflite::gpu::cl::data::Program>>> programs) {
    fbb_.AddOffset(CompiledCache::VT_PROGRAMS, programs);
  }
  explicit CompiledCacheBuilder(::flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  ::flatbuffers::Offset<CompiledCache> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = ::flatbuffers::Offset<CompiledCache>(end);
    return o;
  }
};

inline ::flatbuffers::Offset<CompiledCache> CreateCompiledCache(
    ::flatbuffers::FlatBufferBuilder &_fbb,
    ::flatbuffers::Offset<::flatbuffers::String> driver_version = 0,
    ::flatbuffers::Offset<::flatbuffers::Vector<::flatbuffers::Offset<tflite::gpu::cl::data::Program>>> programs = 0) {
  CompiledCacheBuilder builder_(_fbb);
  builder_.add_programs(programs);
  builder_.add_driver_version(driver_version);
  return builder_.Finish();
}

inline ::flatbuffers::Offset<CompiledCache> CreateCompiledCacheDirect(
    ::flatbuffers::FlatBufferBuilder &_fbb,
    const char *driver_version = nullptr,
    const std::vector<::flatbuffers::Offset<tflite::gpu::cl::data::Program>> *programs = nullptr) {
  auto driver_version__ = driver_version ? _fbb.CreateString(driver_version) : 0;
  auto programs__ = programs ? _fbb.CreateVector<::flatbuffers::Offset<tflite::gpu::cl::data::Program>>(*programs) : 0;
  return tflite::gpu::cl::data::CreateCompiledCache(
      _fbb,
      driver_version__,
      programs__);
}

inline const tflite::gpu::cl::data::CompiledCache *GetCompiledCache(const void *buf) {
  return ::flatbuffers::GetRoot<tflite::gpu::cl::data::CompiledCache>(buf);
}

inline const tflite::gpu::cl::data::CompiledCache *GetSizePrefixedCompiledCache(const void *buf) {
  return ::flatbuffers::GetSizePrefixedRoot<tflite::gpu::cl::data::CompiledCache>(buf);
}

inline const char *CompiledCacheIdentifier() {
  return "AFCM";
}

inline bool CompiledCacheBufferHasIdentifier(const void *buf) {
  return ::flatbuffers::BufferHasIdentifier(
      buf, CompiledCacheIdentifier());
}

inline bool SizePrefixedCompiledCacheBufferHasIdentifier(const void *buf) {
  return ::flatbuffers::BufferHasIdentifier(
      buf, CompiledCacheIdentifier(), true);
}

inline bool VerifyCompiledCacheBuffer(
    ::flatbuffers::Verifier &verifier) {
  return verifier.VerifyBuffer<tflite::gpu::cl::data::CompiledCache>(CompiledCacheIdentifier());
}

inline bool VerifySizePrefixedCompiledCacheBuffer(
    ::flatbuffers::Verifier &verifier) {
  return verifier.VerifySizePrefixedBuffer<tflite::gpu::cl::data::CompiledCache>(CompiledCacheIdentifier());
}

inline const char *CompiledCacheExtension() {
  return "jetbin";
}

inline void FinishCompiledCacheBuffer(
    ::flatbuffers::FlatBufferBuilder &fbb,
    ::flatbuffers::Offset<tflite::gpu::cl::data::CompiledCache> root) {
  fbb.Finish(root, CompiledCacheIdentifier());
}

inline void FinishSizePrefixedCompiledCacheBuffer(
    ::flatbuffers::FlatBufferBuilder &fbb,
    ::flatbuffers::Offset<tflite::gpu::cl::data::CompiledCache> root) {
  fbb.FinishSizePrefixed(root, CompiledCacheIdentifier());
}

}  // namespace data
}  // namespace cl
}  // namespace gpu
}  // namespace tflite

#endif  // FLATBUFFERS_GENERATED_COMPILEDPROGRAMCACHE_TFLITE_GPU_CL_DATA_H_

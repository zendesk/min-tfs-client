/* Copyright 2018 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow/c/env.h"

#include "tensorflow/c/c_api.h"
#include "tensorflow/core/lib/io/path.h"
#include "tensorflow/core/platform/test.h"
#include "tensorflow/core/platform/types.h"

#define ASSERT_TF_OK(x) ASSERT_EQ(TF_OK, TF_GetCode(x))

TEST(TestEnv, TestDirHandling) {
  TF_StringStream* tempdirs = TF_GetLocalTempDirectories();
  const char* tempdir;
  bool found = false;
  while (TF_StringStreamNext(tempdirs, &tempdir)) {
    found = true;

    TF_Status* s = TF_NewStatus();

    ::tensorflow::string dirpath =
        ::tensorflow::io::JoinPath(tempdir, "somedir");
    TF_CreateDir(dirpath.c_str(), s);
    ASSERT_TF_OK(s) << "TF_CreateDir failed for " << dirpath << ": "
                    << TF_Message(s);

    ::tensorflow::string filepath =
        ::tensorflow::io::JoinPath(dirpath, "somefile.txt");
    TF_WritableFileHandle* handle;
    TF_NewWritableFile(filepath.c_str(), &handle, s);
    ASSERT_TF_OK(s) << "NewWritableFile failed for " << filepath << ": "
                    << TF_Message(s);

    const char* data = "Hello, world!\n";
    TF_AppendWritableFile(handle, data, strlen(data), s);
    ASSERT_TF_OK(s) << "TF_AppendWritableFile failed to append data to file at "
                    << filepath << ": " << TF_Message(s);

    TF_CloseWritableFile(handle, s);
    ASSERT_TF_OK(s) << "TF_CloseWritableFile failed to close handle to "
                    << filepath << ": " << TF_Message(s);

    TF_StringStream* children = TF_GetChildren(dirpath.c_str(), s);
    ASSERT_TF_OK(s) << "TF_GetChildren failed for " << dirpath;
    const char* childpath;
    ASSERT_TRUE(TF_StringStreamNext(children, &childpath));
    ASSERT_EQ(::tensorflow::string(childpath), "somefile.txt");
    // There should only be one file in this directory.
    ASSERT_FALSE(TF_StringStreamNext(children, &childpath));
    ASSERT_EQ(childpath, nullptr);
    TF_StringStreamDone(children);

    TF_FileStatistics stats;
    TF_FileStat(filepath.c_str(), &stats, s);
    ASSERT_EQ(stats.length, strlen(data));
    ASSERT_FALSE(stats.is_directory);
    ASSERT_GT(stats.mtime_nsec, 0);

    // Trying to delete a non-empty directory should fail.
    TF_DeleteDir(dirpath.c_str(), s);
    ASSERT_NE(TF_OK, TF_GetCode(s))
        << "TF_DeleteDir unexpectedly succeeded with a non-empty directory "
        << dirpath;

    TF_DeleteFile(filepath.c_str(), s);
    ASSERT_TF_OK(s) << "TF_DeleteFile failed for " << filepath << ": "
                    << TF_Message(s);

    // Now deleting the directory should work.
    TF_DeleteDir(dirpath.c_str(), s);
    ASSERT_TF_OK(s) << "TF_DeleteDir failed for " << dirpath << ": "
                    << TF_Message(s);

    TF_DeleteStatus(s);
    break;
  }

  ASSERT_TRUE(found) << "expected at least one temp dir";

  TF_StringStreamDone(tempdirs);
}

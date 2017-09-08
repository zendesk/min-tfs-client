/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

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
#ifndef THIRD_PARTY_TENSORFLOW_CORE_KERNELS_SQL_SQLITE_QUERY_CONNECTION_H_
#define THIRD_PARTY_TENSORFLOW_CORE_KERNELS_SQL_SQLITE_QUERY_CONNECTION_H_

#include "sqlite3.h"
#include "tensorflow/core/kernels/sql/query_connection.h"

namespace tensorflow {

namespace sql {

class SqliteQueryConnection : public QueryConnection {
 public:
  SqliteQueryConnection();
  ~SqliteQueryConnection() override;
  Status Open(const string& data_source_name, const string& query,
              const DataTypeVector& output_types) override;
  Status Close() override;
  Status GetNext(std::vector<Tensor>* out_tensors,
                 bool* end_of_sequence) override;

 private:
  // Executes the query string `query_`.
  Status ExecuteQuery();
  // Fills `tensor` with the column_index_th element of the current row of
  // `stmt_`.
  void FillTensorWithResultSetEntry(const DataType& data_type, int column_index,
                                    Tensor* tensor);
  sqlite3* db_ = nullptr;
  sqlite3_stmt* stmt_ = nullptr;
  int column_count_ = 0;
  string query_;
  DataTypeVector output_types_;
};

}  // namespace sql

}  // namespace tensorflow

#endif  // THIRD_PARTY_TENSORFLOW_CORE_KERNELS_SQL_SQLITE_QUERY_CONNECTION_H_

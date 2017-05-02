/* Copyright 2015 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow/core/kernels/lookup_table_op.h"
#define EIGEN_USE_THREADS

#include <string>
#include <type_traits>
#include <utility>

#include "tensorflow/core/framework/register_types.h"
#include "tensorflow/core/framework/types.h"
#include "tensorflow/core/kernels/initializable_lookup_table.h"
#include "tensorflow/core/lib/gtl/inlined_vector.h"
#include "tensorflow/core/lib/hash/hash.h"

namespace tensorflow {
namespace lookup {

// Lookup table that wraps an unordered_map, where the key and value data type
// is specified. Each individual value must be a scalar. If vector values are
// required, use MutableHashTableOfTensors.
//
// This table is mutable and thread safe - Insert can be called at any time.
//
// Sample use case:
//
// MutableHashTableOfScalars<int64, int64> table;  // int64 -> int64.
// // Populate the table, elements could be added in one or multiple calls.
// table.Insert(key_tensor, value_tensor); // Populate the table.
//
// table.Find(in_t, &out_t, default_t)
//
template <class K, class V>
class MutableHashTableOfScalars final : public LookupInterface {
 public:
  MutableHashTableOfScalars(OpKernelContext* ctx, OpKernel* kernel) {}

  size_t size() const override {
    mutex_lock l(mu_);
    return table_.size();
  }

  Status Find(OpKernelContext* ctx, const Tensor& key, Tensor* value,
              const Tensor& default_value) override {
    const V default_val = default_value.flat<V>()(0);
    const auto key_values = key.flat<K>();
    auto value_values = value->flat<V>();

    mutex_lock l(mu_);
    for (int64 i = 0; i < key_values.size(); ++i) {
      value_values(i) = gtl::FindWithDefault(
          table_, SubtleMustCopyUnlessStringOrFloat(key_values(i)),
          default_val);
    }

    return Status::OK();
  }

  Status DoInsert(bool clear, const Tensor& keys, const Tensor& values) {
    const auto key_values = keys.flat<K>();
    const auto value_values = values.flat<V>();

    mutex_lock l(mu_);
    if (clear) {
      table_.clear();
    }
    for (int64 i = 0; i < key_values.size(); ++i) {
      gtl::InsertOrUpdate(&table_,
                          SubtleMustCopyUnlessStringOrFloat(key_values(i)),
                          SubtleMustCopyUnlessStringOrFloat(value_values(i)));
    }
    return Status::OK();
  }

  Status Insert(OpKernelContext* ctx, const Tensor& keys,
                const Tensor& values) override {
    return DoInsert(false, keys, values);
  }

  Status ImportValues(OpKernelContext* ctx, const Tensor& keys,
                      const Tensor& values) override {
    return DoInsert(true, keys, values);
  }

  Status ExportValues(OpKernelContext* ctx) override {
    mutex_lock l(mu_);
    int64 size = table_.size();

    Tensor* keys;
    Tensor* values;
    TF_RETURN_IF_ERROR(
        ctx->allocate_output("keys", TensorShape({size}), &keys));
    TF_RETURN_IF_ERROR(
        ctx->allocate_output("values", TensorShape({size}), &values));

    auto keys_data = keys->flat<K>();
    auto values_data = values->flat<V>();
    int64 i = 0;
    for (auto it = table_.begin(); it != table_.end(); ++it, ++i) {
      keys_data(i) = it->first;
      values_data(i) = it->second;
    }
    return Status::OK();
  }

  DataType key_dtype() const override { return DataTypeToEnum<K>::v(); }

  DataType value_dtype() const override { return DataTypeToEnum<V>::v(); }

  TensorShape key_shape() const final { return TensorShape(); }

  TensorShape value_shape() const override { return TensorShape(); }

 private:
  // TODO(andreasst): consider using a read/write lock or a concurrent map
  mutable mutex mu_;
  std::unordered_map<K, V> table_ GUARDED_BY(mu_);
};

// Lookup table that wraps an unordered_map. Behaves identical to
// MutableHashTableOfScalars except that each value must be a vector.
template <class K, class V>
class MutableHashTableOfTensors final : public LookupInterface {
 public:
  MutableHashTableOfTensors(OpKernelContext* ctx, OpKernel* kernel) {
    OP_REQUIRES_OK(ctx,
                   GetNodeAttr(kernel->def(), "value_shape", &value_shape_));
    OP_REQUIRES(
        ctx, TensorShapeUtils::IsVector(value_shape_),
        errors::InvalidArgument("Default value must be a vector, got shape ",
                                value_shape_.DebugString()));
  }

  size_t size() const override {
    mutex_lock l(mu_);
    return table_.size();
  }

  Status Find(OpKernelContext* ctx, const Tensor& key, Tensor* value,
              const Tensor& default_value) override {
    const auto default_flat = default_value.flat<V>();
    const auto key_values = key.flat<K>();
    auto value_values = value->flat_inner_dims<V, 2>();
    int64 value_dim = value_shape_.dim_size(0);

    mutex_lock l(mu_);
    for (int64 i = 0; i < key_values.size(); ++i) {
      ValueArray* value_vec = gtl::FindOrNull(
          table_, SubtleMustCopyUnlessStringOrFloat(key_values(i)));
      if (value_vec != nullptr) {
        for (int64 j = 0; j < value_dim; j++) {
          value_values(i, j) = value_vec->at(j);
        }
      } else {
        for (int64 j = 0; j < value_dim; j++) {
          value_values(i, j) = default_flat(j);
        }
      }
    }

    return Status::OK();
  }

  Status DoInsert(bool clear, const Tensor& keys, const Tensor& values) {
    const auto key_values = keys.flat<K>();
    const auto value_values = values.flat_inner_dims<V, 2>();
    int64 value_dim = value_shape_.dim_size(0);

    mutex_lock l(mu_);
    if (clear) {
      table_.clear();
    }
    for (int64 i = 0; i < key_values.size(); ++i) {
      ValueArray value_vec;
      for (int64 j = 0; j < value_dim; j++) {
        V value = value_values(i, j);
        value_vec.push_back(value);
      }
      gtl::InsertOrUpdate(
          &table_, SubtleMustCopyUnlessStringOrFloat(key_values(i)), value_vec);
    }
    return Status::OK();
  }

  Status Insert(OpKernelContext* ctx, const Tensor& keys,
                const Tensor& values) override {
    return DoInsert(false, keys, values);
  }

  Status ImportValues(OpKernelContext* ctx, const Tensor& keys,
                      const Tensor& values) override {
    return DoInsert(true, keys, values);
  }

  Status ExportValues(OpKernelContext* ctx) override {
    mutex_lock l(mu_);
    int64 size = table_.size();
    int64 value_dim = value_shape_.dim_size(0);

    Tensor* keys;
    Tensor* values;
    TF_RETURN_IF_ERROR(
        ctx->allocate_output("keys", TensorShape({size}), &keys));
    TF_RETURN_IF_ERROR(ctx->allocate_output(
        "values", TensorShape({size, value_dim}), &values));

    auto keys_data = keys->flat<K>();
    auto values_data = values->matrix<V>();
    int64 i = 0;
    for (auto it = table_.begin(); it != table_.end(); ++it, ++i) {
      K key = it->first;
      ValueArray value = it->second;
      keys_data(i) = key;
      for (int64 j = 0; j < value_dim; j++) {
        values_data(i, j) = value[j];
      }
    }
    return Status::OK();
  }

  DataType key_dtype() const override { return DataTypeToEnum<K>::v(); }

  DataType value_dtype() const override { return DataTypeToEnum<V>::v(); }

  TensorShape key_shape() const final { return TensorShape(); }

  TensorShape value_shape() const override { return value_shape_; }

 private:
  TensorShape value_shape_;
  // TODO(andreasst): consider using a read/write lock or a concurrent map
  mutable mutex mu_;
  typedef gtl::InlinedVector<V, 4> ValueArray;
  std::unordered_map<K, ValueArray> table_ GUARDED_BY(mu_);
};

namespace {

template <typename T>
inline uint64 HashScalar(const T& key) {
  return static_cast<uint64>(key);
}

inline uint64 HashScalar(const string& key) { return Hash64(key); }

// If the given shape is a scalar return {1} instead. Otherwise leave it alone.
TensorShape MaybeVectorizeShape(const TensorShape& shape) {
  if (shape.dims() == 0) {
    return TensorShape({1});
  }
  return shape;
}

}  // namespace

// Modeled after densehashtable in https://github.com/sparsehash/sparsehash
template <class K, class V>
class MutableDenseHashTable final : public LookupInterface {
 public:
  MutableDenseHashTable(OpKernelContext* ctx, OpKernel* kernel) {
    OP_REQUIRES_OK(
        ctx, GetNodeAttr(kernel->def(), "max_load_factor", &max_load_factor_));
    OP_REQUIRES(ctx, max_load_factor_ > 0 && max_load_factor_ < 1,
                errors::InvalidArgument(
                    "max_load_factor must be between 0 and 1, got: ",
                    max_load_factor_));

    OP_REQUIRES_OK(ctx,
                   GetNodeAttr(kernel->def(), "value_shape", &value_shape_));
    OP_REQUIRES(ctx,
                TensorShapeUtils::IsScalar(value_shape_) ||
                    TensorShapeUtils::IsVector(value_shape_),
                errors::InvalidArgument(
                    "Empty value must be a scalar or a vector, got shape ",
                    value_shape_.DebugString()));

    const Tensor* empty_key_input;
    OP_REQUIRES_OK(ctx, ctx->input("empty_key", &empty_key_input));
    key_shape_ = empty_key_input->shape();
    OP_REQUIRES(ctx,
                TensorShapeUtils::IsScalar(key_shape_) ||
                    TensorShapeUtils::IsVector(key_shape_),
                errors::InvalidArgument(
                    "Empty key must be a scalar or a vector, got shape ",
                    key_shape_.DebugString()));
    empty_key_ = PersistentTensor(*empty_key_input);
    empty_key_hash_ = HashKey(
        empty_key_input->template shaped<K, 2>({1, key_shape_.num_elements()}),
        0);

    int64 initial_num_buckets;
    OP_REQUIRES_OK(ctx, GetNodeAttr(kernel->def(), "initial_num_buckets",
                                    &initial_num_buckets));
    OP_REQUIRES_OK(ctx, AllocateBuckets(ctx, initial_num_buckets));
  }

  size_t size() const override LOCKS_EXCLUDED(mu_) {
    mutex_lock l(mu_);
    return num_entries_;
  }

  Status Find(OpKernelContext* ctx, const Tensor& key, Tensor* value,
              const Tensor& default_value) override LOCKS_EXCLUDED(mu_) {
    const int64 num_elements = key.dim_size(0);
    const int64 key_size = key_shape_.num_elements();
    const int64 value_size = value_shape_.num_elements();
    if (key.NumElements() != num_elements * key_size) {
      TensorShape expected_shape({num_elements});
      expected_shape.AppendShape(key_shape_);
      return errors::InvalidArgument("Expected key shape ",
                                     expected_shape.DebugString(), " got ",
                                     key.shape().DebugString());
    }
    const auto key_matrix = key.shaped<K, 2>({num_elements, key_size});
    auto value_matrix = value->shaped<V, 2>({num_elements, value_size});
    const auto default_flat = default_value.flat<V>();

    mutex_lock l(mu_);
    const auto key_buckets_matrix =
        key_buckets_.AccessTensor(ctx)->template matrix<K>();
    const auto value_buckets_matrix =
        value_buckets_.AccessTensor(ctx)->template matrix<V>();
    const auto empty_key_matrix =
        empty_key_.AccessTensor(ctx)->template shaped<K, 2>({1, key_size});
    const int64 bit_mask = num_buckets_ - 1;
    // TODO(andreasst): parallelize using work_sharder
    for (int64 i = 0; i < num_elements; ++i) {
      const uint64 key_hash = HashKey(key_matrix, i);
      if (empty_key_hash_ == key_hash &&
          IsEqualKey(empty_key_matrix, 0, key_matrix, i)) {
        return errors::InvalidArgument(
            "Using the empty_key as a table key is not allowed");
      }
      int64 bucket_index = key_hash & bit_mask;
      int64 num_probes = 0;
      while (true) {
        if (IsEqualKey(key_buckets_matrix, bucket_index, key_matrix, i)) {
          for (int64 j = 0; j < value_size; ++j) {
            // TODO(andreasst): check if we can get rid of SubtleMustCopy
            // here and elsewhere in this file.
            value_matrix(i, j) = SubtleMustCopyUnlessStringOrFloat(
                value_buckets_matrix(bucket_index, j));
          }
          break;
        }
        if (IsEqualKey(key_buckets_matrix, bucket_index, empty_key_matrix, 0)) {
          for (int64 j = 0; j < value_size; ++j) {
            value_matrix(i, j) =
                SubtleMustCopyUnlessStringOrFloat(default_flat(j));
          }
          break;
        }
        ++num_probes;
        bucket_index =
            (bucket_index + num_probes) & bit_mask;  // quadratic probing
        if (num_probes >= num_buckets_) {
          return errors::Internal(
              "Internal error in MutableDenseHashTable lookup");
        }
      }
    }
    return Status::OK();
  }

  Status Insert(OpKernelContext* ctx, const Tensor& key,
                const Tensor& value) override LOCKS_EXCLUDED(mu_) {
    if (key.NumElements() != key.dim_size(0) * key_shape_.num_elements()) {
      TensorShape expected_shape({key.dim_size(0)});
      expected_shape.AppendShape(key_shape_);
      return errors::InvalidArgument("Expected key shape ",
                                     expected_shape.DebugString(), " got ",
                                     key.shape().DebugString());
    }
    mutex_lock l(mu_);
    // For simplicity we assume that all keys in the input result in inserts
    // rather than updates. That means we may grow the table even though we
    // don't need to. As long as the number of keys inserted in one call is
    // small compared to the size of the map, the impact of this is minimal.
    const int64 pending_num_entries = num_entries_ + key.dim_size(0);
    if (pending_num_entries > num_buckets_ * max_load_factor_) {
      int64 new_num_buckets = num_buckets_;
      do {
        new_num_buckets <<= 1;
      } while (pending_num_entries > new_num_buckets * max_load_factor_);
      TF_RETURN_IF_ERROR(Rebucket(ctx, new_num_buckets));
    }
    return DoInsert(ctx, key, value, false);
  }

  Status ImportValues(OpKernelContext* ctx, const Tensor& keys,
                      const Tensor& values) override LOCKS_EXCLUDED(mu_) {
    mutex_lock l(mu_);
    num_buckets_ = keys.dim_size(0);
    key_buckets_ = PersistentTensor(keys);
    value_buckets_ = PersistentTensor(values);
    // Count the number of keys that are not the empty_key. This requires
    // iterating through the whole table but that is OK as we only execute it
    // during checkpoint restore.
    num_entries_ = 0;
    const auto empty_key_tensor =
        empty_key_.AccessTensor(ctx)->template shaped<K, 2>(
            {1, key_shape_.num_elements()});
    const auto key_buckets_tensor =
        key_buckets_.AccessTensor(ctx)->template matrix<K>();
    for (int64 i = 0; i < num_buckets_; ++i) {
      if (!IsEqualKey(key_buckets_tensor, i, empty_key_tensor, 0)) {
        ++num_entries_;
      }
    }
    return Status::OK();
  }

  Status ExportValues(OpKernelContext* ctx) override LOCKS_EXCLUDED(mu_) {
    mutex_lock l(mu_);
    Tensor key_buckets_tensor = *key_buckets_.AccessTensor(ctx);
    Tensor value_buckets_tensor = *value_buckets_.AccessTensor(ctx);
    TF_RETURN_IF_ERROR(ctx->set_output("keys", key_buckets_tensor));
    TF_RETURN_IF_ERROR(ctx->set_output("values", value_buckets_tensor));
    return Status::OK();
  }

  Status CheckKeyAndValueTensorsForImport(const Tensor& keys,
                                          const Tensor& values) override {
    TF_RETURN_IF_ERROR(CheckKeyAndValueTypes(keys, values));
    TF_RETURN_IF_ERROR(CheckKeyShape(keys.shape()));

    // The storage format in key_buckets_ and value_buckets_ is always vectors,
    // even if the inputs are scalars. This is what eventually gets exported
    // and is expected by the import method as well.
    TensorShape key_shape = MaybeVectorizeShape(key_shape_);
    TensorShape value_shape = MaybeVectorizeShape(value_shape_);

    // Compute the final expected shape of the value by starting with the shape
    // of all keys, removing the dimensions particular to each key and then
    // appending the shape of a single value.
    TensorShape expected_value_shape = keys.shape();
    for (int i = 0; i < key_shape.dims(); ++i) {
      expected_value_shape.RemoveDim(expected_value_shape.dims() - 1);
    }
    expected_value_shape.AppendShape(value_shape);
    if (values.shape() != expected_value_shape) {
      return errors::InvalidArgument(
          "Expected shape ", expected_value_shape.DebugString(),
          " for value, got ", values.shape().DebugString());
    }
    return Status::OK();
  }

  DataType key_dtype() const override { return DataTypeToEnum<K>::v(); }

  DataType value_dtype() const override { return DataTypeToEnum<V>::v(); }

  TensorShape key_shape() const override { return key_shape_; }

  TensorShape value_shape() const override { return value_shape_; }

 private:
  Status DoInsert(OpKernelContext* ctx, const Tensor& key, const Tensor& value,
                  bool ignore_empty_key) EXCLUSIVE_LOCKS_REQUIRED(mu_) {
    const int64 num_elements = key.dim_size(0);
    const int64 value_size = value_shape_.num_elements();
    const int64 key_size = key_shape_.num_elements();
    const auto key_matrix = key.shaped<K, 2>({num_elements, key_size});
    auto value_matrix = value.shaped<V, 2>({num_elements, value_size});

    auto key_buckets_matrix =
        key_buckets_.AccessTensor(ctx)->template matrix<K>();
    auto value_buckets_matrix =
        value_buckets_.AccessTensor(ctx)->template matrix<V>();
    const auto empty_key_tensor =
        empty_key_.AccessTensor(ctx)->template shaped<K, 2>({1, key_size});
    const int64 bit_mask = num_buckets_ - 1;
    for (int64 i = 0; i < num_elements; ++i) {
      const uint64 key_hash = HashKey(key_matrix, i);
      if (empty_key_hash_ == key_hash &&
          IsEqualKey(empty_key_tensor, 0, key_matrix, i)) {
        if (ignore_empty_key) {
          continue;
        }
        return errors::InvalidArgument(
            "Using the empty_key as a table key is not allowed");
      }
      int64 bucket_index = key_hash & bit_mask;
      int64 num_probes = 0;
      while (true) {
        if (IsEqualKey(key_buckets_matrix, bucket_index, key_matrix, i)) {
          for (int64 j = 0; j < value_size; ++j) {
            value_buckets_matrix(bucket_index, j) =
                SubtleMustCopyUnlessStringOrFloat(value_matrix(i, j));
          }
          break;
        }
        if (IsEqualKey(key_buckets_matrix, bucket_index, empty_key_tensor, 0)) {
          ++num_entries_;
          for (int64 j = 0; j < key_size; ++j) {
            key_buckets_matrix(bucket_index, j) =
                SubtleMustCopyUnlessStringOrFloat(key_matrix(i, j));
          }
          for (int64 j = 0; j < value_size; ++j) {
            value_buckets_matrix(bucket_index, j) =
                SubtleMustCopyUnlessStringOrFloat(value_matrix(i, j));
          }
          break;
        }
        ++num_probes;
        bucket_index =
            (bucket_index + num_probes) & bit_mask;  // quadratic probing
        if (num_probes >= num_buckets_) {
          return errors::Internal(
              "Internal error in MutableDenseHashTable insert");
        }
      }
    }
    return Status::OK();
  }

  Status AllocateBuckets(OpKernelContext* ctx, int64 new_num_buckets)
      EXCLUSIVE_LOCKS_REQUIRED(mu_) {
    if (new_num_buckets < 4 ||
        ((new_num_buckets & (new_num_buckets - 1)) != 0)) {
      return errors::InvalidArgument(
          "Number of buckets must be at least 4 and a power of 2, got: ",
          new_num_buckets);
    }
    num_buckets_ = new_num_buckets;
    num_entries_ = 0;

    const int64 key_size = key_shape_.num_elements();
    Tensor* key_buckets_tensor;
    TF_RETURN_IF_ERROR(ctx->allocate_persistent(
        key_dtype(), TensorShape({num_buckets_, key_size}), &key_buckets_,
        &key_buckets_tensor));
    auto key_buckets_matrix = key_buckets_tensor->matrix<K>();
    const auto empty_key_flat =
        empty_key_.AccessTensor(ctx)->template flat<K>();
    for (int64 i = 0; i < num_buckets_; ++i) {
      for (int64 j = 0; j < key_size; ++j) {
        key_buckets_matrix(i, j) = empty_key_flat(j);
      }
    }

    const int64 value_size = value_shape_.num_elements();
    Tensor* value_buckets_tensor;
    TF_RETURN_IF_ERROR(ctx->allocate_persistent(
        value_dtype(), TensorShape({num_buckets_, value_size}), &value_buckets_,
        &value_buckets_tensor));
    auto value_buckets_matrix = value_buckets_tensor->matrix<V>();
    for (int64 i = 0; i < num_buckets_; ++i) {
      for (int64 j = 0; j < value_size; ++j) {
        // Initialize values to the default value for the type to avoid
        // exposing uninitialized memory in ExportValues().
        value_buckets_matrix(i, j) = V();
      }
    }
    return Status::OK();
  }

  Status Rebucket(OpKernelContext* ctx, int64 num_new_buckets)
      EXCLUSIVE_LOCKS_REQUIRED(mu_) {
    Tensor old_key_buckets = *key_buckets_.AccessTensor(ctx);
    Tensor old_value_buckets = *value_buckets_.AccessTensor(ctx);
    TF_RETURN_IF_ERROR(AllocateBuckets(ctx, num_new_buckets));
    return DoInsert(ctx, old_key_buckets, old_value_buckets, true);
  }

  uint64 HashKey(typename TTypes<K>::ConstMatrix key, int64 index) const {
    if (key_shape_.num_elements() == 1) {
      return HashScalar(key(index, 0));
    }
    uint64 result = 0;
    for (int64 i = 0; i < key_shape_.num_elements(); ++i) {
      result = Hash64Combine(result, HashScalar(key(index, i)));
    }
    return result;
  }

  // Use a template to allow this function to be used both with Matrix and
  // ConstMatrix types.
  template <typename MT2>
  bool IsEqualKey(typename TTypes<K>::Matrix tensor1, int64 index1, MT2 tensor2,
                  int64 index2) const {
    for (int64 i = 0; i < key_shape_.num_elements(); ++i) {
      if (tensor1(index1, i) != tensor2(index2, i)) {
        return false;
      }
    }
    return true;
  }

  TensorShape key_shape_;
  TensorShape value_shape_;
  float max_load_factor_;
  mutable mutex mu_;
  int64 num_entries_ GUARDED_BY(mu_);
  int64 num_buckets_ GUARDED_BY(mu_);
  PersistentTensor key_buckets_ GUARDED_BY(mu_);
  PersistentTensor value_buckets_ GUARDED_BY(mu_);
  PersistentTensor empty_key_;
  uint64 empty_key_hash_;
};

}  // namespace lookup

// Table lookup op. Perform the lookup operation on the given table.
class LookupTableFindOp : public OpKernel {
 public:
  explicit LookupTableFindOp(OpKernelConstruction* ctx) : OpKernel(ctx) {}

  void Compute(OpKernelContext* ctx) override {
    lookup::LookupInterface* table;
    OP_REQUIRES_OK(ctx, GetLookupTable("table_handle", ctx, &table));
    core::ScopedUnref unref_me(table);

    // Input 0 could be a STRING_REF or a RESOURCE
    DataType expected_input_0 =
        (ctx->input_dtype(0) == DT_RESOURCE) ? DT_RESOURCE : DT_STRING_REF;
    DataTypeVector expected_inputs = {expected_input_0, table->key_dtype(),
                                      table->value_dtype()};
    DataTypeVector expected_outputs = {table->value_dtype()};
    OP_REQUIRES_OK(ctx, ctx->MatchSignature(expected_inputs, expected_outputs));

    const Tensor& key = ctx->input(1);
    const Tensor& default_value = ctx->input(2);
    OP_REQUIRES_OK(ctx, table->CheckFindArguments(key, default_value));

    TensorShape output_shape = key.shape();
    for (int i = table->key_shape().dims(); i > 0; --i) {
      output_shape.RemoveDim(output_shape.dims() - 1);
    }
    output_shape.AppendShape(table->value_shape());
    Tensor* out;
    OP_REQUIRES_OK(ctx, ctx->allocate_output("values", output_shape, &out));

    OP_REQUIRES_OK(ctx, table->Find(ctx, key, out, default_value));
  }
};

REGISTER_KERNEL_BUILDER(Name("LookupTableFind").Device(DEVICE_CPU),
                        LookupTableFindOp);
REGISTER_KERNEL_BUILDER(Name("LookupTableFindV2").Device(DEVICE_CPU),
                        LookupTableFindOp);

// Table insert op.
class LookupTableInsertOp : public OpKernel {
 public:
  explicit LookupTableInsertOp(OpKernelConstruction* ctx) : OpKernel(ctx) {}

  void Compute(OpKernelContext* ctx) override {
    lookup::LookupInterface* table;
    OP_REQUIRES_OK(ctx, GetLookupTable("table_handle", ctx, &table));
    core::ScopedUnref unref_me(table);

    DataType expected_input_0 =
        (ctx->input_dtype(0) == DT_RESOURCE) ? DT_RESOURCE : DT_STRING_REF;
    DataTypeVector expected_inputs = {expected_input_0, table->key_dtype(),
                                      table->value_dtype()};
    OP_REQUIRES_OK(ctx, ctx->MatchSignature(expected_inputs, {}));

    const Tensor& keys = ctx->input(1);
    const Tensor& values = ctx->input(2);
    OP_REQUIRES_OK(ctx, table->CheckKeyAndValueTensorsForInsert(keys, values));

    int64 memory_used_before = 0;
    if (ctx->track_allocations()) {
      memory_used_before = table->MemoryUsed();
    }
    OP_REQUIRES_OK(ctx, table->Insert(ctx, keys, values));
    if (ctx->track_allocations()) {
      ctx->record_host_persistent_memory_allocation(table->MemoryUsed() -
                                                    memory_used_before);
    }
  }
};

REGISTER_KERNEL_BUILDER(Name("LookupTableInsert").Device(DEVICE_CPU),
                        LookupTableInsertOp);
REGISTER_KERNEL_BUILDER(Name("LookupTableInsertV2").Device(DEVICE_CPU),
                        LookupTableInsertOp);

// Op that returns the size of the given table.
class LookupTableSizeOp : public OpKernel {
 public:
  explicit LookupTableSizeOp(OpKernelConstruction* ctx) : OpKernel(ctx) {}

  void Compute(OpKernelContext* ctx) override {
    lookup::LookupInterface* table;
    OP_REQUIRES_OK(ctx, GetLookupTable("table_handle", ctx, &table));
    core::ScopedUnref unref_me(table);

    Tensor* out;
    OP_REQUIRES_OK(ctx, ctx->allocate_output("size", TensorShape({}), &out));
    out->flat<int64>().setConstant(table->size());
  }
};

REGISTER_KERNEL_BUILDER(Name("LookupTableSize").Device(DEVICE_CPU),
                        LookupTableSizeOp);
REGISTER_KERNEL_BUILDER(Name("LookupTableSizeV2").Device(DEVICE_CPU),
                        LookupTableSizeOp);

// Op that outputs tensors of all keys and all values.
class LookupTableExportOp : public OpKernel {
 public:
  explicit LookupTableExportOp(OpKernelConstruction* ctx) : OpKernel(ctx) {}

  void Compute(OpKernelContext* ctx) override {
    lookup::LookupInterface* table;
    OP_REQUIRES_OK(ctx, GetLookupTable("table_handle", ctx, &table));
    core::ScopedUnref unref_me(table);

    OP_REQUIRES_OK(ctx, table->ExportValues(ctx));
  }
};

REGISTER_KERNEL_BUILDER(Name("LookupTableExport").Device(DEVICE_CPU),
                        LookupTableExportOp);
REGISTER_KERNEL_BUILDER(Name("LookupTableExportV2").Device(DEVICE_CPU),
                        LookupTableExportOp);

// Clear the table and insert data.
class LookupTableImportOp : public OpKernel {
 public:
  explicit LookupTableImportOp(OpKernelConstruction* ctx) : OpKernel(ctx) {}

  void Compute(OpKernelContext* ctx) override {
    lookup::LookupInterface* table;
    OP_REQUIRES_OK(ctx, GetLookupTable("table_handle", ctx, &table));
    core::ScopedUnref unref_me(table);

    DataType expected_input_0 =
        (ctx->input_dtype(0) == DT_RESOURCE) ? DT_RESOURCE : DT_STRING_REF;
    DataTypeVector expected_inputs = {expected_input_0, table->key_dtype(),
                                      table->value_dtype()};
    OP_REQUIRES_OK(ctx, ctx->MatchSignature(expected_inputs, {}));

    const Tensor& keys = ctx->input(1);
    const Tensor& values = ctx->input(2);
    OP_REQUIRES_OK(ctx, table->CheckKeyAndValueTensorsForImport(keys, values));

    int memory_used_before = 0;
    if (ctx->track_allocations()) {
      memory_used_before = table->MemoryUsed();
    }
    OP_REQUIRES_OK(ctx, table->ImportValues(ctx, keys, values));
    if (ctx->track_allocations()) {
      ctx->record_host_persistent_memory_allocation(table->MemoryUsed() -
                                                    memory_used_before);
    }
  }
};

REGISTER_KERNEL_BUILDER(Name("LookupTableImport").Device(DEVICE_CPU),
                        LookupTableImportOp);
REGISTER_KERNEL_BUILDER(Name("LookupTableImportV2").Device(DEVICE_CPU),
                        LookupTableImportOp);

// Register the HashTable op with the currently supported key and value types.
#define REGISTER_KERNEL(key_dtype, value_dtype)                           \
  REGISTER_KERNEL_BUILDER(                                                \
      Name("HashTable")                                                   \
          .Device(DEVICE_CPU)                                             \
          .TypeConstraint<key_dtype>("key_dtype")                         \
          .TypeConstraint<value_dtype>("value_dtype"),                    \
      LookupTableOp<lookup::HashTable<key_dtype, value_dtype>, key_dtype, \
                    value_dtype>)                                         \
  REGISTER_KERNEL_BUILDER(                                                \
      Name("HashTableV2")                                                 \
          .Device(DEVICE_CPU)                                             \
          .TypeConstraint<key_dtype>("key_dtype")                         \
          .TypeConstraint<value_dtype>("value_dtype"),                    \
      LookupTableOp<lookup::HashTable<key_dtype, value_dtype>, key_dtype, \
                    value_dtype>)

REGISTER_KERNEL(string, double);
REGISTER_KERNEL(string, float);
REGISTER_KERNEL(string, int32);
REGISTER_KERNEL(string, int64);
REGISTER_KERNEL(int64, string);
REGISTER_KERNEL(int64, int64);
REGISTER_KERNEL(string, string);
REGISTER_KERNEL(string, bool);

#undef REGISTER_KERNEL

// Register the MutableHashTable op.
#define REGISTER_KERNEL(key_dtype, value_dtype)                                \
  REGISTER_KERNEL_BUILDER(                                                     \
      Name("MutableHashTable")                                                 \
          .Device(DEVICE_CPU)                                                  \
          .TypeConstraint<key_dtype>("key_dtype")                              \
          .TypeConstraint<value_dtype>("value_dtype"),                         \
      LookupTableOp<lookup::MutableHashTableOfScalars<key_dtype, value_dtype>, \
                    key_dtype, value_dtype>)                                   \
  REGISTER_KERNEL_BUILDER(                                                     \
      Name("MutableHashTableV2")                                               \
          .Device(DEVICE_CPU)                                                  \
          .TypeConstraint<key_dtype>("key_dtype")                              \
          .TypeConstraint<value_dtype>("value_dtype"),                         \
      LookupTableOp<lookup::MutableHashTableOfScalars<key_dtype, value_dtype>, \
                    key_dtype, value_dtype>)

REGISTER_KERNEL(string, float);
REGISTER_KERNEL(string, int64);
REGISTER_KERNEL(int64, string);
REGISTER_KERNEL(string, bool);
REGISTER_KERNEL(int64, float);

#undef REGISTER_KERNEL

// Register the MutableHashTableOfTensors op.
#define REGISTER_KERNEL(key_dtype, value_dtype)                                \
  REGISTER_KERNEL_BUILDER(                                                     \
      Name("MutableHashTableOfTensors")                                        \
          .Device(DEVICE_CPU)                                                  \
          .TypeConstraint<key_dtype>("key_dtype")                              \
          .TypeConstraint<value_dtype>("value_dtype"),                         \
      LookupTableOp<lookup::MutableHashTableOfTensors<key_dtype, value_dtype>, \
                    key_dtype, value_dtype>)                                   \
  REGISTER_KERNEL_BUILDER(                                                     \
      Name("MutableHashTableOfTensorsV2")                                      \
          .Device(DEVICE_CPU)                                                  \
          .TypeConstraint<key_dtype>("key_dtype")                              \
          .TypeConstraint<value_dtype>("value_dtype"),                         \
      LookupTableOp<lookup::MutableHashTableOfTensors<key_dtype, value_dtype>, \
                    key_dtype, value_dtype>)

REGISTER_KERNEL(string, float);
REGISTER_KERNEL(string, int64);
REGISTER_KERNEL(int64, string);
REGISTER_KERNEL(string, bool);

#undef REGISTER_KERNEL

// Register the MutableDenseHashTable op.
#define REGISTER_KERNEL(key_dtype, value_dtype)                            \
  REGISTER_KERNEL_BUILDER(                                                 \
      Name("MutableDenseHashTable")                                        \
          .Device(DEVICE_CPU)                                              \
          .TypeConstraint<key_dtype>("key_dtype")                          \
          .TypeConstraint<value_dtype>("value_dtype"),                     \
      LookupTableOp<lookup::MutableDenseHashTable<key_dtype, value_dtype>, \
                    key_dtype, value_dtype>)                               \
  REGISTER_KERNEL_BUILDER(                                                 \
      Name("MutableDenseHashTableV2")                                      \
          .Device(DEVICE_CPU)                                              \
          .TypeConstraint<key_dtype>("key_dtype")                          \
          .TypeConstraint<value_dtype>("value_dtype"),                     \
      LookupTableOp<lookup::MutableDenseHashTable<key_dtype, value_dtype>, \
                    key_dtype, value_dtype>)

REGISTER_KERNEL(int64, int64);
REGISTER_KERNEL(int64, float);
REGISTER_KERNEL(int64, double);
REGISTER_KERNEL(string, float);
REGISTER_KERNEL(string, bool);
REGISTER_KERNEL(int64, bool);

#undef REGISTER_KERNEL

}  // namespace tensorflow

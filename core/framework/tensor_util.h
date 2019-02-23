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

#ifndef TENSORFLOW_CORE_FRAMEWORK_TENSOR_UTIL_H_
#define TENSORFLOW_CORE_FRAMEWORK_TENSOR_UTIL_H_

#include <algorithm>
#include <vector>
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/tensor.pb.h"
#include "tensorflow/core/framework/tensor_shape.pb.h"
#include "tensorflow/core/platform/protobuf.h"
#include "tensorflow/core/platform/types.h"

namespace tensorflow {
namespace tensor {

// DeepCopy returns a tensor whose contents are a deep copy of the
// contents of 'other'.  This function is intended only for
// convenience, not speed.
//
// REQUIRES: 'other' must point to data stored in CPU memory.
// REQUIRES: 'other' must be a Tensor of a copy-able type if
//           'other' is not appropriately memory-aligned.
Tensor DeepCopy(const Tensor& other);

// Concatenates 'tensors' into a single tensor, along their 0th dimension.
//
// REQUIRES: All members of 'tensors' must have the same data type parameter.
// REQUIRES: Each member of 'tensors' must have at least one dimension.
// REQUIRES: Each member of 'tensors' must point to data stored in CPU memory.
// REQUIRES: Each member of 'tensors' must be a Tensor of a copy-able type if it
//           is not appropriately memory-aligned.
Status Concat(const gtl::ArraySlice<Tensor>& tensors,
              Tensor* result) TF_MUST_USE_RESULT;

// Splits 'tensor' into 'sizes.size()' individual tensors, along the 0th
// dimension. The ith output tensor has 0th-dimension size 'sizes[i]'.
//
// REQUIRES: 'tensor' must have at least one dimension.
// REQUIRES: 'tensor.dim_size(0)' must equal the sum of the elements of 'sizes'.
// REQUIRES: 'tensor' must point to data stored in CPU memory.
// REQUIRES: 'tensor' must be a Tensor of a copy-able type if it is not
//           appropriately memory-aligned.
//
// Split() and Concat() are inverse operations.
Status Split(const Tensor& tensor, const gtl::ArraySlice<int64>& sizes,
             std::vector<Tensor>* result) TF_MUST_USE_RESULT;

namespace internal {
void SetTensorProtoShape(std::vector<size_t> shape,
                         TensorShapeProto* shape_proto);

template <typename Type>
class TensorProtoFieldHelper : public std::false_type {};

#define DEFINE_PROTO_FIELD_HELPER(TYPE, FIELDNAME)                            \
  template <>                                                                 \
  class TensorProtoFieldHelper<TYPE> : public std::true_type {                \
   public:                                                                    \
    typedef decltype(                                                         \
        std::declval<TensorProto>().FIELDNAME##_val(0)) FieldType;            \
    typedef decltype(                                                         \
        std::declval<TensorProto>().FIELDNAME##_val()) RepeatedFieldType;     \
    typedef decltype(std::declval<TensorProto>().mutable_##FIELDNAME##_val()) \
        MutableRepeatedFieldType;                                             \
    static MutableRepeatedFieldType GetMutableField(TensorProto* proto) {     \
      return proto->mutable_##FIELDNAME##_val();                              \
    }                                                                         \
    static RepeatedFieldType& GetField(const TensorProto& proto) {            \
      return proto.FIELDNAME##_val();                                         \
    }                                                                         \
  }

DEFINE_PROTO_FIELD_HELPER(float, float);
DEFINE_PROTO_FIELD_HELPER(double, double);
DEFINE_PROTO_FIELD_HELPER(int8, int);
DEFINE_PROTO_FIELD_HELPER(uint8, int);
DEFINE_PROTO_FIELD_HELPER(int16, int);
DEFINE_PROTO_FIELD_HELPER(uint16, int);
DEFINE_PROTO_FIELD_HELPER(int32, int);
DEFINE_PROTO_FIELD_HELPER(uint32, uint32);
DEFINE_PROTO_FIELD_HELPER(int64, int64);
DEFINE_PROTO_FIELD_HELPER(uint64, uint64);
DEFINE_PROTO_FIELD_HELPER(bool, bool);
DEFINE_PROTO_FIELD_HELPER(qint8, int);
DEFINE_PROTO_FIELD_HELPER(quint8, int);
DEFINE_PROTO_FIELD_HELPER(qint16, int);
DEFINE_PROTO_FIELD_HELPER(quint16, int);
DEFINE_PROTO_FIELD_HELPER(qint32, int);
// TODO(rmlarsen): Add support for complex and half float types.
// DEFINE_PROTO_FIELD_HELPER(Eigen::hals, half);
// DEFINE_PROTO_FIELD_HELPER(qint32, half);

#undef DEFINE_PROTO_HELPER

template <typename T>
class TensorProtoHelper : public std::true_type {
 public:
  using FieldHelper = TensorProtoFieldHelper<T>;
  using FieldType = typename TensorProtoFieldHelper<T>::FieldType;

  static DataType GetDataType() { return DataTypeToEnum<T>::value; }

  static size_t NumValues(const TensorProto& proto) {
    return FieldHelper::GetField(proto).size();
  }

  static void AddValue(const T& value, TensorProto* proto) {
    FieldHelper::GetMutableField(proto)->Add(static_cast<FieldType>(value));
  }

  static T GetValue(size_t index, const TensorProto& proto) {
    return static_cast<T>(FieldHelper::GetField(proto).Get(index));
  }

  template <typename IterType>
  static void AddValues(IterType begin, IterType end, TensorProto* proto) {
    using SrcType = typename std::iterator_traits<IterType>::value_type;
    size_t n = std::distance(begin, end);
    FieldType* dst_ptr = AppendUninitialized(n, proto);
    if (std::is_same<SrcType, FieldType>::value) {
      std::copy(begin, end, dst_ptr);
    } else {
      std::transform(begin, end, dst_ptr, [](const SrcType& x) -> FieldType {
        return static_cast<FieldType>(x);
      });
    }
  }

  template <typename IterType>
  static void CopyValues(IterType dst, const TensorProto& proto) {
    using DstType = typename std::iterator_traits<IterType>::value_type;
    auto begin = FieldHelper::GetField(proto).begin();
    auto end = FieldHelper::GetField(proto).end();
    if (std::is_same<DstType, FieldType>::value) {
      std::copy(begin, end, dst);
    } else {
      std::transform(begin, end, dst, [](const FieldType& x) -> DstType {
        return static_cast<DstType>(x);
      });
    }
  }

  static void Truncate(size_t new_size, TensorProto* proto) {
    FieldHelper::GetMutableField(proto)->Truncate(new_size);
  }

  static FieldType* AppendUninitialized(size_t n, TensorProto* proto) {
    auto* field = FieldHelper::GetMutableField(proto);
    field->Reserve(field->size() + n);
    return reinterpret_cast<FieldType*>(field->AddNAlreadyReserved(n));
  }
};

// Specialization for string.
template <>
class TensorProtoHelper<string> : public std::true_type {
 public:
  static DataType GetDataType() { return DataType::DT_STRING; }
  static void AddValue(const string& value, TensorProto* proto) {
    *proto->mutable_string_val()->Add() = value;
  }
  template <typename IterType>
  static void AddValues(IterType begin, IterType end, TensorProto* proto) {
    for (IterType it = begin; it != end; ++it) {
      AddValue(*it, proto);
    }
  }
  template <typename IterType>
  static void CopyToTensorContent(IterType begin, IterType end,
                                  TensorProto* proto) {
    AddValues(begin, end, proto);
  }
};

}  // namespace internal

// Creates a 'TensorProto' with specified shape and values.
// The dtype and a field to represent data values of the returned 'TensorProto'
// are determined based on type of the 'values' parameter.
template <typename Type>
typename std::enable_if<internal::TensorProtoHelper<Type>::value,
                        TensorProto>::type
CreateTensorProto(const std::vector<Type>& values,
                  const std::vector<size_t>& shape) {
  TensorProto tensor;
  TensorShapeProto tensor_shape_proto;
  internal::SetTensorProtoShape(shape, &tensor_shape_proto);
  if (TensorShape(tensor_shape_proto).num_elements() != values.size()) {
    LOG(ERROR) << "Shape and number of values (" << values.size()
               << ") are incompatible.";
    return tensor;
  }
  using TypeHelper = internal::TensorProtoHelper<Type>;
  tensor.set_dtype(TypeHelper::GetDataType());
  tensor.mutable_tensor_shape()->Swap(&tensor_shape_proto);
  TypeHelper::AddValues(values.begin(), values.end(), &tensor);
  return tensor;
}

// Converts values in tensor to run-length encoded compressed form.
//
// The elements of a tensor can be stored in a TensorProto in one of the
// following two forms:
// 1. As a raw byte string in the field `tensor_content` containing the
//    serialized in-memory representation of the tensor.
// 2. As values of a repeated field depending on the datatype, e.g. that
//    values of a DT_FLOAT tensor would be stored in the repeated field
//    `float_val`.
// Storage scheme 2 may use a simple form of run-length encoding to compress
// data: If the values contains a tail of identical values, the repeated field
// will be truncated such that the number of values in the repeated field is
// less than the number of elements implied by the field`tensor_shape`. The
// original tensor can be recovered by repeating the final value in the repeated
// field.
//
// The TensorProto will be compressed if a) the tensor contains at least
// min_num_elements elements and b) the compressed tensor proto is would be at
// most the size of the original tensor proto divided by min_compression_ratio.
//
// Returns true if the tensor was compressed.
bool CompressTensorProtoInPlace(int64 min_num_elements,
                                float min_compression_ratio,
                                TensorProto* tensor);

inline bool CompressTensorProtoInPlace(TensorProto* tensor) {
  static const int64 kDefaultMinNumElements = 64;
  static const float kDefaultMinCompressionRatio = 2.0f;
  return CompressTensorProtoInPlace(kDefaultMinNumElements,
                                    kDefaultMinCompressionRatio, tensor);
}

}  // namespace tensor
}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_FRAMEWORK_TENSOR_UTIL_H_

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

#include "tensorflow/compiler/xla/service/cpu/llvm_ir_runtime.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "tensorflow/compiler/xla/service/cpu/vector_support_library.h"
#include "tensorflow/compiler/xla/service/llvm_ir/math_ops.h"
#include "tensorflow/core/platform/logging.h"

namespace xla {
namespace cpu {
namespace runtime {

const char* const kTanhV4F32SymbolName = "__xla_cpu_runtime_TanhV4F32";
const char* const kTanhV8F32SymbolName = "__xla_cpu_runtime_TanhV8F32";
const char* const kTanhV16F32SymbolName = "__xla_cpu_runtime_TanhV16F32";
const char* const kExpV4F32SymbolName = "__xla_cpu_runtime_ExpV4F32";
const char* const kExpV8F32SymbolName = "__xla_cpu_runtime_ExpV8F32";
const char* const kExpV16F32SymbolName = "__xla_cpu_runtime_ExpV16F32";
const char* const kLogV4F32SymbolName = "__xla_cpu_runtime_LogV4F32AVX";
const char* const kLogV8F32SymbolName = "__xla_cpu_runtime_LogV8F32AVX";
const char* const kLogV16F32SymbolName = "__xla_cpu_runtime_LogV16F32AVX";

namespace {

// Replaces calls to the function `fn_name` with the code generated by
// fn_body_generator.
//
// We assume that fn_name accepts either a scalar f32 or a vector of
// vector_width f32s, and that fn_body_generator generates a function body with
// the same inputs/outputs as fn_name.
void RewriteCalls(
    llvm::Module* module, const char* fn_name,
    std::function<llvm::Value*(llvm::IRBuilder<>* b, llvm::Value* input,
                               int32 vector_width)>
        fn_body_generator,
    int32 vector_width, bool enable_fast_math) {
  llvm::Function* fn = module->getFunction(fn_name);
  if (fn == nullptr) {
    // If the function declaration is not present in the module, there can't be
    // any calls to resolve.  Don't emit the function in this case.
    return;
  }

  // Our task is to generate a function body for `fn`, but we can't generate a
  // function body for an LLVM intrinsic. So if fn is an intrinsic, replace it
  // with a new function.
  if (fn->isIntrinsic()) {
    llvm::Function* new_fn = llvm::Function::Create(
        fn->getFunctionType(), llvm::GlobalValue::InternalLinkage,
        llvm::Twine("xla_impl.") + fn_name, module);
    fn->replaceAllUsesWith(new_fn);
    fn->eraseFromParent();
    fn = new_fn;
  }

  llvm::LLVMContext* context = &module->getContext();

  llvm::BasicBlock* fn_body = llvm::BasicBlock::Create(*context, "body", fn);
  llvm::IRBuilder<> b(fn_body);
  llvm::FastMathFlags fast_math_flags;
  fast_math_flags.setFast(enable_fast_math);
  b.setFastMathFlags(fast_math_flags);

  llvm::Value* input = &*fn->arg_begin();

  // Upcast to vector type if input is a scalar.
  if (vector_width == 1) {
    llvm::Type* v1_type = llvm::VectorType::get(input->getType(), 1);
    input = b.CreateInsertElement(llvm::UndefValue::get(v1_type), input,
                                  uint64_t{0});
  }

  // Generate the vectorized code.
  CHECK_EQ(vector_width, input->getType()->getVectorNumElements());
  llvm::Value* result = fn_body_generator(&b, input, vector_width);

  // Downcast result to scalar type if necessary.
  if (vector_width == 1) {
    result = b.CreateExtractElement(result, uint64_t{0});
  }
  b.CreateRet(result);
  DCHECK(!llvm::verifyFunction(*fn));

  // Force-inline `fn` into all of its callers and then delete `fn`.
  //
  // TODO(b/73081976): Should we avoid inlining these in some cases?
  std::vector<llvm::CallInst*> calls_to_inline;
  for (auto* user : fn->users()) {
    calls_to_inline.push_back(llvm::cast<llvm::CallInst>(user));
  }
  for (auto* call_to_inline : calls_to_inline) {
    llvm::InlineFunctionInfo inline_function_info;
    CHECK(llvm::InlineFunction(call_to_inline, inline_function_info));
  }
  fn->eraseFromParent();
}

llvm::Value* GenerateVF32Tanh(llvm::IRBuilder<>* b, llvm::Value* input,
                              int32 /*vector_width*/) {
  return llvm_ir::EmitFastTanh(b, input);
}

llvm::Value* GenerateVF32Exp(llvm::IRBuilder<>* b, llvm::Value* input,
                             int32 vector_width) {
  VectorSupportLibrary vsl(F32, vector_width, b, "exp_f32");

  // This implements the same polynomial approximation as implemented in Cephes.
  const llvm::APFloat half = GetIeeeF32(0.5);
  const llvm::APFloat one = GetIeeeF32(1);

  const llvm::APFloat cephes_LOG2EF = GetIeeeF32(1.44269504088896341);
  const llvm::APFloat cephes_exp_C1 = GetIeeeF32(0.693359375);
  const llvm::APFloat cephes_exp_C2 = GetIeeeF32(-2.12194440e-4);

  const llvm::APFloat cephes_exp_p0 = GetIeeeF32(1.9875691500E-4);
  const llvm::APFloat cephes_exp_p1 = GetIeeeF32(1.3981999507E-3);
  const llvm::APFloat cephes_exp_p2 = GetIeeeF32(8.3334519073E-3);
  const llvm::APFloat cephes_exp_p3 = GetIeeeF32(4.1665795894E-2);
  const llvm::APFloat cephes_exp_p4 = GetIeeeF32(1.6666665459E-1);
  const llvm::APFloat cephes_exp_p5 = GetIeeeF32(5.0000001201E-1);

  // To compute e^input, we re-express it as
  //
  //   e^input = e^(a + b)
  //           = e^(a + n log(2))
  //           = e^a * 2^n.
  //
  // We choose n = floor(a * log(2) + 0.5), restricting the value of `a` to
  // (-0.5, 0.5).  We then use a polynomial to compute e^a.

  // Restrict input to a small range, including some values that evaluate to
  // +/- inf.  Our computations below aren't particularly sensitive to the exact
  // choices here, so we choose values a bit larger/smaller than
  //
  //   log(F32_MAX) =       88.723...
  //   log(F32_EPSILON) = -103.279....
  //
  input = vsl.Clamp(input, GetIeeeF32(-104), GetIeeeF32(88.8));

  llvm::Value* x = input;
  llvm::Value* n = vsl.Floor(vsl.MulAdd(input, cephes_LOG2EF, half));

  // When we eventually do the multiplication in e^a * 2^n, we need to handle
  // the case when n > 127, the max fp32 exponent (so 2^n == inf) but e^a < 1
  // (so e^a * 2^n != inf).  There's a similar problem for n < -126, the
  // smallest fp32 exponent.
  //
  // A straightforward solution would be to detect n out of range and split it
  // up, doing
  //
  //   e^a * 2^n = e^a * 2^(n1 + n2)
  //             = (2^n1 * e^a) * 2^n2.
  //
  // But it turns out this approach is quite slow.  It's not clear why; our
  // hypothesis is that the integer operations on the exponent `n` have nonlocal
  // effects on the pipeline.
  //
  // The approach we use instead is to clamp n to [-126, 127] so 2^n doesn't
  // over/underflow.  This causes `a` to be outside the range (-0.5, 0.5), which
  // means that our polynomial for e^a will give a less-accurate result.  In
  // practice this seems to work well enough; it passes our exhaustive tests,
  // breaking only one result, and by one ulp (we return exp(88.7228394) =
  // max-float but we should return inf).
  n = vsl.Clamp(n, GetIeeeF32(-126), GetIeeeF32(127));

  // Polynomial to compute z = e^a, accurate for a in (-0.5, 0.5).
  x = vsl.Sub(x, vsl.Mul(cephes_exp_C1, n));
  x = vsl.Sub(x, vsl.Mul(cephes_exp_C2, n));
  llvm::Value* z = vsl.MulAdd(x, cephes_exp_p0, cephes_exp_p1);
  z = vsl.MulAdd(z, x, cephes_exp_p2);
  z = vsl.MulAdd(z, x, cephes_exp_p3);
  z = vsl.MulAdd(z, x, cephes_exp_p4);
  z = vsl.MulAdd(z, x, cephes_exp_p5);
  z = vsl.MulAdd(z, vsl.Mul(x, x), x);
  z = vsl.Add(one, z);

  // Convert n to an i32.  This is safe because we clamped it above.
  llvm::Value* n_i32 =
      b->CreateFPToSI(n, llvm::VectorType::get(b->getInt32Ty(), vector_width));

  // Create 2^n as an fp32.  This works because -126 <= n <= 127 means that n is
  // within the bounds for an fp32 exponent.
  auto splat_i32 = [&](int32 v) {
    return b->CreateVectorSplat(vector_width, b->getInt32(v));
  };
  const int32 kF32SignificandBits = 23;
  llvm::Value* exp_bias = splat_i32(0x7f);
  llvm::Value* pow2 =
      b->CreateBitCast(b->CreateShl(b->CreateAdd(n_i32, exp_bias),
                                    splat_i32(kF32SignificandBits)),
                       vsl.vector_type());

  // Return z * 2^n.
  return vsl.Mul(z, pow2);
}

llvm::Value* GenerateVF32Log(llvm::IRBuilder<>* b, llvm::Value* input,
                             int32 vector_width) {
  VectorSupportLibrary vsl(F32, vector_width, b, "log_f32");

  const llvm::APFloat half = GetIeeeF32(0.5);
  const llvm::APFloat one = GetIeeeF32(1.0);

  // This implements the same polynomial approximation as implemented in Eigen3.
  // Returns NaN for x < 0, -INF for x = 0
  const llvm::APFloat cephes_SQRTHF = GetIeeeF32(0.707106781186547524);
  const llvm::APFloat cephes_log_p0 = GetIeeeF32(7.0376836292E-2);
  const llvm::APFloat cephes_log_p1 = GetIeeeF32(-1.1514610310E-1);
  const llvm::APFloat cephes_log_p2 = GetIeeeF32(1.1676998740E-1);
  const llvm::APFloat cephes_log_p3 = GetIeeeF32(-1.2420140846E-1);
  const llvm::APFloat cephes_log_p4 = GetIeeeF32(+1.4249322787E-1);
  const llvm::APFloat cephes_log_p5 = GetIeeeF32(-1.6668057665E-1);
  const llvm::APFloat cephes_log_p6 = GetIeeeF32(+2.0000714765E-1);
  const llvm::APFloat cephes_log_p7 = GetIeeeF32(-2.4999993993E-1);
  const llvm::APFloat cephes_log_p8 = GetIeeeF32(+3.3333331174E-1);
  const llvm::APFloat cephes_log_q1 = GetIeeeF32(-2.12194440e-4);
  const llvm::APFloat cephes_log_q2 = GetIeeeF32(0.693359375);

  // The smallest non denormalized float number.
  const llvm::APFloat min_norm_pos = GetIeeeF32FromBitwiseRep(0x00800000);
  const llvm::APFloat minus_inf = GetIeeeF32FromBitwiseRep(0xff800000);
  const llvm::APFloat pos_inf = GetIeeeF32FromBitwiseRep(0x7f800000);
  const llvm::APFloat inv_mant_mask = GetIeeeF32FromBitwiseRep(~0x7f800000);

  // invalid_mask is set if x is negative or NaN (and therefore output
  // must be NaN).
  llvm::Value* invalid_mask = vsl.FCmpULEMask(input, vsl.GetZeroVector());
  llvm::Value* is_zero_mask = vsl.FCmpEQMask(input, vsl.GetZeroVector());
  llvm::Value* is_pos_inf_mask = vsl.FCmpEQMask(input, pos_inf);

  // Cut off denormalized stuff.
  llvm::Value* tmp0 = vsl.Max(min_norm_pos, input);

  // VectorSupportLibrary (intentionally) can't juggle more than one type at a
  // time so drop down to IRBuilder for this bit.
  llvm::Value* vector_constant_0x7f =
      b->CreateVectorSplat(vector_width, b->getInt32(0x7f));
  llvm::Value* vector_constant_23 =
      b->CreateVectorSplat(vector_width, b->getInt32(23));
  llvm::Type* i32_vector_type =
      llvm::VectorType::get(b->getInt32Ty(), vector_width);

  llvm::Value* emm0 = b->CreateLShr(b->CreateBitCast(tmp0, i32_vector_type),
                                    vector_constant_23);

  // Keep only the fractional part.
  tmp0 = vsl.FloatAnd(tmp0, inv_mant_mask);
  tmp0 = vsl.FloatOr(tmp0, half);

  emm0 = b->CreateSub(emm0, vector_constant_0x7f);
  llvm::Value* e = vsl.Add(one, b->CreateSIToFP(emm0, vsl.vector_type()));

  // part2:
  //   if( x < SQRTHF ) {
  //     e -= 1;
  //     x = x + x - 1.0;
  //   } else { x = x - 1.0; }
  llvm::Value* mask = vsl.FCmpOLTMask(tmp0, cephes_SQRTHF);
  llvm::Value* tmp1 = vsl.FloatAnd(tmp0, mask);
  tmp0 = vsl.Sub(tmp0, one);
  e = vsl.Sub(e, vsl.FloatAnd(mask, one));
  tmp0 = vsl.Add(tmp0, tmp1);

  llvm::Value* x2 = vsl.Mul(tmp0, tmp0);
  llvm::Value* x3 = vsl.Mul(x2, tmp0);

  llvm::Value *y, *y1, *y2;
  y = vsl.MulAdd(tmp0, cephes_log_p0, cephes_log_p1);
  y1 = vsl.MulAdd(tmp0, cephes_log_p3, cephes_log_p4);
  y2 = vsl.MulAdd(tmp0, cephes_log_p6, cephes_log_p7);
  y = vsl.MulAdd(y, tmp0, cephes_log_p2);
  y1 = vsl.MulAdd(y1, tmp0, cephes_log_p5);
  y2 = vsl.MulAdd(y2, tmp0, cephes_log_p8);
  y = vsl.MulAdd(y, x3, y1);
  y = vsl.MulAdd(y, x3, y2);
  y = vsl.Mul(y, x3);

  y1 = vsl.Mul(cephes_log_q1, e);
  llvm::Value* tmp2 = vsl.Mul(half, x2);
  y = vsl.Add(y, y1);
  tmp0 = vsl.Sub(tmp0, tmp2);
  y2 = vsl.Mul(cephes_log_q2, e);
  tmp0 = vsl.Add(tmp0, y);
  tmp0 = vsl.Add(tmp0, y2);

  // Contains +/-inf where +/-inf is the correct answer, otherwise 0.
  llvm::Value* result_inf = vsl.FloatOr(vsl.FloatAnd(is_zero_mask, minus_inf),
                                        vsl.FloatAnd(is_pos_inf_mask, pos_inf));

  // Contains a finite result or nan.  This is the correct answer only if both
  // result_minus_inf and result_pos_inf are both 0.
  //
  // (This implementation works because 0xffffffff is a nan.)
  llvm::Value* result_finite_or_nan = vsl.FloatOr(tmp0, invalid_mask);

  // Combine the above into a final result.
  return vsl.FloatOr(result_inf,
                     vsl.FloatAndNot(vsl.FloatOr(is_zero_mask, is_pos_inf_mask),
                                     result_finite_or_nan));
}
}  // namespace

void RewriteIRRuntimeFunctions(llvm::Module* module, bool enable_fast_math) {
  // Curry some params to RewriteCalls.
  auto rewrite_calls =
      std::bind(RewriteCalls, module, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3, enable_fast_math);

  rewrite_calls("tanhf", GenerateVF32Tanh, /*vector_width=*/1);
  rewrite_calls("llvm.tanh.f32", GenerateVF32Tanh, /*vector_width=*/1);
  rewrite_calls(kTanhV4F32SymbolName, GenerateVF32Tanh, /*vector_width=*/4);
  rewrite_calls(kTanhV8F32SymbolName, GenerateVF32Tanh, /*vector_width=*/8);
  rewrite_calls(kTanhV16F32SymbolName, GenerateVF32Tanh, /*vector_width=*/16);

  rewrite_calls("expf", GenerateVF32Exp, /*vector_width=*/1);
  rewrite_calls("llvm.exp.f32", GenerateVF32Exp, /*vector_width=*/1);
  rewrite_calls(kExpV4F32SymbolName, GenerateVF32Exp, /*vector_width=*/4);
  rewrite_calls(kExpV8F32SymbolName, GenerateVF32Exp, /*vector_width=*/8);
  rewrite_calls(kExpV16F32SymbolName, GenerateVF32Exp, /*vector_width=*/16);

  rewrite_calls("logf", GenerateVF32Log, /*vector_width=*/1);
  rewrite_calls("llvm.log.f32", GenerateVF32Log, /*vector_width=*/1);
  rewrite_calls(kLogV4F32SymbolName, GenerateVF32Log, /*vector_width=*/4);
  rewrite_calls(kLogV8F32SymbolName, GenerateVF32Log, /*vector_width=*/8);
  rewrite_calls(kLogV16F32SymbolName, GenerateVF32Log, /*vector_width=*/16);
}

}  // namespace runtime
}  // namespace cpu
}  // namespace xla

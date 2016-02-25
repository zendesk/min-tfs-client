/* Copyright 2016 Google Inc. All Rights Reserved.

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

#ifndef THIRD_PARTY_TENSORFLOW_CONTRIB_LINEAR_OPTIMIZER_KERNELS_LOGISTIC_LOSS_H_
#define THIRD_PARTY_TENSORFLOW_CONTRIB_LINEAR_OPTIMIZER_KERNELS_LOGISTIC_LOSS_H_

#include <algorithm>
#include <cmath>

namespace tensorflow {
struct logistic_loss {
  // Use an approximate step that is guaranteed to decrease the dual loss.
  // Derivation of this is available in  Page 14 Eq 16 of
  // http://arxiv.org/pdf/1211.2717v1.pdf
  inline static double ComputeUpdatedDual(const double label,
                                          const double example_weight,
                                          const double current_dual,
                                          const double wx,
                                          const double weighted_example_norm,
                                          const double primal_loss,
                                          const double dual_loss) {
    const double ywx = label * wx;
    // To avoid overflow, we compute derivative of logistic loss with respect to
    // log-odds as follows.
    double inverse_exp_term = 0;
    if (ywx > 0) {
      const double exp_minus_ywx = exp(-ywx);
      inverse_exp_term = exp_minus_ywx / (1 + exp_minus_ywx);
    } else {
      inverse_exp_term = 1 / (1 + exp(ywx));
    }
    // f(a) = sup (a*x  - f(x)) then a = f'(x), where a is the aproximate dual.
    const double approximate_dual = inverse_exp_term * label;
    const double delta_dual = approximate_dual - current_dual;
    // Upper bound on the smoothness constant of log loss. This is 0.25 i.e.
    // when log-odds is zero.
    const double gamma =
        (wx == 0) ? 0.25 : (1 - 2 * inverse_exp_term) / (2 * ywx);
    const double wx_dual = wx * current_dual * example_weight;
    const double delta_dual_squared = delta_dual * delta_dual;
    const double smooth_delta_dual_squared = delta_dual_squared * gamma * 0.5;
    double multiplier =
        (primal_loss + dual_loss + wx_dual + smooth_delta_dual_squared) /
        std::max(1.0,
                 delta_dual_squared *
                     (gamma +
                      weighted_example_norm * example_weight * example_weight));
    // Multiplier must be in the range [0, 1].
    multiplier = std::max(std::min(1.0, multiplier), 0.0);
    return current_dual + delta_dual * multiplier;
  }

  // Dual of logisitic loss function.
  // https://en.wikipedia.org/wiki/Convex_conjugate
  inline static double ComputeDualLoss(const double current_dual,
                                       const double example_label,
                                       const double example_weight) {
    // Dual of the logistic loss function is
    // ay * log(ay) + (1-ay) * log (1-ay), where a is the dual variable.
    const double ay = current_dual * example_label;
    const double log_ay = (ay > 0) ? log(ay) : 0;
    const double one_minus_ay = 1 - ay;
    const double log_one_minus_ay = (one_minus_ay > 0) ? log(one_minus_ay) : 0;
    return ((ay * log_ay) + (one_minus_ay * log_one_minus_ay)) * example_weight;
  }

  // Logistic loss for binary classification.
  // https://en.wikipedia.org/wiki/Loss_functions_for_classification
  inline static double ComputePrimalLoss(const double wx,
                                         const double example_label,
                                         const double example_weight) {
    // Logistic loss:
    //   log(1 + e^(-ywx))
    //   log(e^0 + e^(-ywx))
    //   a + log(e^(0-a) + e^(-ywx - a)),  where a is max(0, -ywx)
    // https://hips.seas.harvard.edu/blog/2013/01/09/computing-log-sum-exp/
    const double y_wx = example_label * wx;
    if (y_wx > 0) {
      // 0 + log(e^(0) + e^(-ywx - 0))
      // log(1 + e^(-ywx))
      return log(1 + exp(-y_wx)) * example_weight;
    }
    // -ywx + log(e^(ywx) + e^(-ywx + ywx))
    // log(e^(ywx) + e^(0)) - ywx
    // log(1 + e^(ywx)) - ywx
    return (log(1 + exp(y_wx)) - y_wx) * example_weight;
  }
};
}  // namespace tensorflow

#endif  // THIRD_PARTY_TENSORFLOW_CONTRIB_LINEAR_OPTIMIZER_KERNELS_LOGISTIC_LOSS_H_

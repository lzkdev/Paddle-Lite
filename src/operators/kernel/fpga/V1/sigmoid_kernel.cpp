/* Copyright (c) 2018 PaddlePaddle Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#ifdef SIGMOID_OP

#include "operators/kernel/activation_kernel.h"
namespace paddle_mobile {
namespace operators {

using framework::DDim;
using framework::Tensor;

template <>
bool SigmoidKernel<FPGA, float>::Init(SigmoidParam<FPGA> *param) {
  auto input = const_cast<Tensor *>(param->InputX());
  auto input_ptr = input->data<float>();
  auto out = param->Out();
  fpga::format_fp32_ofm(out);

  auto float_input = new Tensor;
  if (input->dims().size() == 2) {
    float_input->mutable_data<float>({1, input->dims()[1]});
  } else if (input->dims().size() == 4) {
    float_input->mutable_data<float>(
        {1, input->dims()[2], input->dims()[3], input->dims()[1]});
  } else {
    DLOG << "wrong dimension of softmax input";
  }

  fpga::format_fp32_ofm(float_input);
  fpga::BypassArgs args = {fpga::DATA_TYPE_FP16};
  args.input_layout_type = fpga::LAYOUT_HWC;
  args.output_layout_type = fpga::LAYOUT_CHW;
  args.input_data_type = fpga::DATA_TYPE_FP16;
  args.output_data_type = fpga::DATA_TYPE_FP32;
  args.image.address = input_ptr;
  args.image.height =
      (input->dims().size() == 4) ? (uint32_t)input->dims()[2] : 1;
  args.image.width =
      (input->dims().size() == 4) ? (uint32_t)input->dims()[3] : 1;
  args.image.channels = (uint32_t)input->dims()[1];
  args.output.address = float_input->data<float>();
  args.output.scale_address = float_input->scale;
  param->SetFloatInput(float_input);
  param->SetFpgaArgs(args);

  return true;
}
template <typename T>
T Sigmoid(const T a) {
  T tmp = -1.0f * a;
  return (1.0 / (1.0 + exp(tmp)));
}
template <typename T>
void sigmoidFuntor(Tensor *input, Tensor *output) {
  auto *input_ptr = input->data<T>();
  auto *output_ptr = output->mutable_data<T>();
  for (int i = 0; i < input->numel(); i++) {
    *(output_ptr + i) = Sigmoid<T>(*(input_ptr + i));
  }
}
template <>
void SigmoidKernel<FPGA, float>::Compute(const SigmoidParam<FPGA> &param) {
  Tensor *in_x = param.FloatInput();
  Tensor *out = param.Out();

  fpga::PerformBypass(param.FpgaArgs());
  fpga::fpga_invalidate((void *)in_x->data<float>(),  // NOLINT
                        in_x->numel() * sizeof(float));
  // TODO: In general case, 0 should be squeezed before softmax input  // NOLINT
  sigmoidFuntor<float>(in_x, out);
  fpga::fpga_flush(out->data<float>(), out->memory_size());
}
}  // namespace operators
}  // namespace paddle_mobile

#endif

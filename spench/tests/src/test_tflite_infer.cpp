#include "contrib/tflite/tensorflow/lite/interpreter.h"
#include "contrib/tflite/tensorflow/lite/kernels/register.h"
#include "contrib/tflite/tensorflow/lite/model.h"
#include "contrib/tflite/tensorflow/lite/optional_debug_tools.h"

#include "tests/src/utils.h"
#include <catch.hpp>
#include <vector>

TEST_CASE("SUCCEED Run relu model dummy inference [tflite]") {
  std::unique_ptr<tflite::FlatBufferModel> model =
      tflite::FlatBufferModel::BuildFromFile("data/models/dummy_relu_model.tflite");
  REQUIRE(model != nullptr);

  tflite::ops::builtin::BuiltinOpResolver resolver;
  tflite::InterpreterBuilder builder(*model, resolver);
  std::unique_ptr<tflite::Interpreter> interpreter;
  builder(&interpreter);
  REQUIRE(interpreter != nullptr);

  REQUIRE(interpreter->AllocateTensors() == kTfLiteOk);
  tflite::PrintInterpreterState(interpreter.get());

  std::vector<float> input_vector {-1.3, 0.3, 1.0};
  std::vector<float> gt_vector {0, 0.3, 1.0};

  interpreter->AllocateTensors();
  float* input_data_ptr = interpreter->typed_input_tensor<float>(0);
  for (auto const& value : input_vector) {
    *(input_data_ptr) = value;
    input_data_ptr++;
  }

  interpreter->Invoke();

  float* output_data_ptr = interpreter->typed_output_tensor<float>(0);
  std::vector<float> output;
  output.assign(output_data_ptr, output_data_ptr + input_vector.size());

  IsVectorsClose(output, gt_vector, 0.0000001);
}

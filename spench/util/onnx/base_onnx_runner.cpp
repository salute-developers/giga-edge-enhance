#include "spench/util/onnx/base_onnx_runner.h"

#ifndef BUILD_WITHOUT_MODEL_ENCRYPTION
#include "spench/submodules/modelencryption/lib/include/crypto.h"
#endif

namespace util {
namespace onnx {

BaseOnnxRunner::BaseOnnxRunner()
    : run_options_ptr_(std::make_unique<Ort::RunOptions>(nullptr)) {}

BaseOnnxRunner::BaseOnnxRunner(const std::vector<std::string>& inputs,
                               const std::vector<std::string>& outputs,
                               const std::shared_ptr<spench::NetworkConfig> config,
                               const std::string& password,
                               const std::string& checksum,
                               const uint32_t num_threads,
                               bool gpu)
    : run_options_ptr_(std::make_unique<Ort::RunOptions>(nullptr)) {
  init_model(config, password,
            checksum, num_threads,
            gpu);
  validate_model(inputs, outputs);
}

BaseOnnxRunner::~BaseOnnxRunner() {
  for (size_t i = 0; i < model_input_names_.size(); ++i) {
    default_allocator_.Free(model_input_names_[i]);
  }
  for (size_t i = 0; i < model_output_names_.size(); ++i) {
    default_allocator_.Free(model_output_names_[i]);
  }
  session_ptr_.reset();
  run_options_ptr_.reset();
}

void BaseOnnxRunner::init_model(const std::shared_ptr<spench::NetworkConfig> config,
                               const std::string& password,
                               const std::string& checksum,
                               const uint32_t num_threads,
                               bool gpu) {
  if (!gpu) {
    std::lock_guard<std::mutex> lock(get_cpu_session_mutex());
    const auto& session_options = get_ort_session_options_cpu(num_threads, false);

    try {
      if (session_ptr_ == nullptr) {
        if (!password.empty() || !checksum.empty()) {
#ifndef BUILD_WITHOUT_MODEL_ENCRYPTION
          modelencryption::Decryptor decryptor;
          modelencryption::Hasher hasher;
          std::vector<uint8_t> model_data;

          std::unique_ptr<modelencryption::Checksum> license_checksum = nullptr;
          if (!checksum.empty()) {
            license_checksum.reset(new modelencryption::Checksum(hasher.DeserializeChecksum(checksum.c_str())));
          }
          if (config->is_model_bundled()) {
            model_data = decryptor.DecryptArray(config->get_model(),
                                                modelencryption::Key(password),
                                                license_checksum.get());
          } else {
            model_data = decryptor.DecryptFile(config->model_path(),
                                              modelencryption::Key(password),
                                              license_checksum.get());
          }

          session_ptr_ = std::make_unique<Ort::Session>(util::onnx::get_ort_env(),
                                                         model_data.data(),
                                                         model_data.size(),
                                                         session_options);
#else
          throw OnnxValidationException("Password must be empty or library should be built with encryption support");
#endif
        } else {
          if (config->is_model_bundled()) {
            const auto& model_data = config->get_model();
            session_ptr_ = std::make_unique<Ort::Session>(util::onnx::get_ort_env(),
                                                            model_data.data(),
                                                            model_data.size(),
                                                            session_options);
          } else {
            session_ptr_ = std::make_unique<Ort::Session>(util::onnx::get_ort_env(),
                                                            config->model_path().c_str(),
                                                            session_options);
          }
        }
      }
    } catch (const std::exception& ex) {
      std::string err_message = std::string("Model reading error. Detailes: ")
                                + ex.what();
      throw OnnxValidationException(err_message);
    }
    memory_info_ptr_ = const_cast<Ort::MemoryInfo*>(&get_memory_info_cpu());
  } else {
    std::lock_guard<std::mutex> lock(get_gpu_session_mutex());
    const auto& session_options = get_ort_session_options_gpu(num_threads, false);
    try {
      if (session_ptr_ == nullptr) {
        session_ptr_ = std::make_unique<Ort::Session>(util::onnx::get_ort_env(),
                                                        config->model_path().c_str(),
                                                        session_options);
      }
    } catch (...) {
      throw OnnxValidationException("Model reading error. File does not exist!");
    }
    memory_info_ptr_ = const_cast<Ort::MemoryInfo*>(&get_memory_info_gpu());
  }
}

int32_t BaseOnnxRunner::get_single_dynamic_axis(const std::vector<int64_t>& shape, const char* tensor_debug_name) {
  int32_t found = -1;
  for (size_t i = 0; i < shape.size(); ++i) {
    if (shape[i] < 0) {
      if (found >= 0) {
        std::stringstream iss;
        iss << "Tensor has more than one dynamic dim (-1): " << tensor_debug_name;
        // throw OnnxValidationException(iss.str());
      }
      found = static_cast<int32_t>(i);
    }
  }
  return found;
}

void BaseOnnxRunner::validate_model(const std::vector<std::string>& input_names,
                                   const std::vector<std::string>& output_names) {
  if (!session_ptr_) {
    std::stringstream iss;
    iss << "The model has not defined session or allocator";
    throw OnnxValidationException(iss.str());
  }

  // Inputs
  const size_t inputs_count = session_ptr_->GetInputCount();
  if (inputs_count != input_names.size()) {
    std::stringstream iss;
    iss << "The model has " << inputs_count << " inputs, but you provided " << input_names.size();
    throw OnnxValidationException(iss.str());
  }

  bool inputs_names_same = true;
  for (size_t i = 0; i < inputs_count; ++i) {
    model_input_names_.push_back(session_ptr_->GetInputNameAllocated(i, default_allocator_).release());
    model_input_shapes_.push_back(session_ptr_->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
    const std::string cur_model_input_name = model_input_names_.back();
    if (input_names[i] != cur_model_input_name) {
      inputs_names_same = false;
    }
  }

  // Cache time axis (only dynamic dim) for each input tensor.
  time_axis_in_.resize(model_input_shapes_.size(), -1);
  for (size_t i = 0; i < model_input_shapes_.size(); ++i) {
    // English comment: dynamic time axis is encoded as -1 in ONNX shapes
    time_axis_in_[i] = get_single_dynamic_axis(model_input_shapes_[i], "input");
  }

  if (!inputs_names_same) {
    std::stringstream iss;
    iss << "The model has inputs inconsisted with provided ones";
    throw OnnxValidationException(iss.str());
  }

  // Outputs
  const size_t outputs_count = session_ptr_->GetOutputCount();
  if (outputs_count != output_names.size()) {
    std::stringstream iss;
    iss << "The model has " << outputs_count << " outputs, but you provided " << output_names.size();
    throw OnnxValidationException(iss.str());
  }

  bool outputs_names_same = true;
  for (size_t i = 0; i < outputs_count; ++i) {
    model_output_names_.push_back(session_ptr_->GetOutputNameAllocated(i, default_allocator_).release());
    model_output_shapes_.push_back(session_ptr_->GetOutputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
    const std::string cur_model_output_name = model_output_names_.back();
    if (output_names[i] != cur_model_output_name) {
      outputs_names_same = false;
    }
  }

  if (!outputs_names_same) {
    std::stringstream iss;
    iss << "The model has outputs inconsisted with provided ones";
    throw OnnxValidationException(iss.str());
  }
}

void BaseOnnxRunner::resize_if_needed_in_place(std::vector<float>& to_check,
                                          const std::vector<int64_t>& shape,
                                          uint32_t num_parts) {
  if (to_check.size() == 0) {
    uint64_t multi = std::accumulate(shape.begin(),
                                     shape.end(),
                                     uint64_t{1},
                                     std::multiplies<uint64_t>());
    to_check.resize(multi / num_parts, 0.0f);
  }
}

void BaseOnnxRunner::resize_if_needed_in_place(std::valarray<float>& to_check,
                                          const std::vector<int64_t>& shape,
                                          uint32_t num_parts) {
  if (to_check.size() == 0) {
    uint64_t multi = std::accumulate(shape.begin(),
                                     shape.end(),
                                     uint64_t{1},
                                     std::multiplies<uint64_t>());
    to_check.resize(multi / num_parts, 0.0f);
  }
}

}  // namespace onnx
}  // namespace util

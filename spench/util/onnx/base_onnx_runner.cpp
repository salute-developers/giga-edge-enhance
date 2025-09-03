#include "spench/util/onnx/base_onnx_runner.h"

#ifndef BUILD_WITHOUT_MODEL_ENCRYPTION
#include "spench/submodules/modelencryption/lib/include/crypto.h"
#endif

namespace util {
namespace onnx {

BaseOnnxRunner::BaseOnnxRunner()
    : m_run_options_ptr(std::make_unique<Ort::RunOptions>(nullptr)) {}

BaseOnnxRunner::BaseOnnxRunner(const std::vector<std::string>& inputs,
                               const std::vector<std::string>& outputs,
                               const std::string& model_path,
                               const std::string& password,
                               const std::string& checksum,
                               const uint32_t num_threads,
                               bool gpu)
    : m_run_options_ptr(std::make_unique<Ort::RunOptions>(nullptr)) {

  InitModel(model_path, password,
            checksum, num_threads,
            gpu);
  ValidateModel(inputs, outputs);
}

BaseOnnxRunner::~BaseOnnxRunner() {
  for (size_t i = 0; i < m_model_input_names.size(); ++i) {
    m_default_allocator.Free(m_model_input_names[i]);
  }
  for (size_t i = 0; i < m_model_output_names.size(); ++i) {
    m_default_allocator.Free(m_model_output_names[i]);
  }
  m_session_ptr.reset();
  m_run_options_ptr.reset();
}

void BaseOnnxRunner::InitModel(const std::string& model_path,
                               const std::string& password,
                               const std::string& checksum,
                               const uint32_t num_threads,
                               bool gpu) {
  if (!gpu) {
    std::lock_guard<std::mutex> lock(GetCpuSessionMutex());
    const auto& session_options = GetOrtSessionOptionsCpu(num_threads, false);

    try {
      if (m_session_ptr == nullptr) {
        if (!password.empty() || !checksum.empty()) {
#ifndef BUILD_WITHOUT_MODEL_ENCRYPTION
          modelencryption::Decryptor decryptor;
          modelencryption::Hasher hasher;

          std::unique_ptr<modelencryption::Checksum> license_checksum = nullptr;
          if (!checksum.empty()) {
            license_checksum.reset(new modelencryption::Checksum(hasher.DeserializeChecksum(checksum.c_str())));
          }
          std::vector<uint8_t> modelData =
                              decryptor.DecryptFile(model_path, modelencryption::Key(password), license_checksum.get());

          m_session_ptr = std::make_unique<Ort::Session>(util::onnx::GetOrtEnv(),
                                                         modelData.data(),
                                                         modelData.size(),
                                                         session_options);
#else
          throw OnnxValidationException("Password must be empty or library should be built with encryption support");
#endif
        } else {
          m_session_ptr = std::make_unique<Ort::Session>(util::onnx::GetOrtEnv(),
                                                         model_path.c_str(),
                                                         session_options);
        }
      }
    } catch (const std::exception& ex) {
      std::string err_message = std::string("Model reading error. Detailes: ")
                                + ex.what();
      throw OnnxValidationException(err_message);
    }
    m_memory_info_ptr = const_cast<Ort::MemoryInfo*>(&GetMemoryInfoCpu());
  } else {
    std::lock_guard<std::mutex> lock(GetGpuSessionMutex());
    const auto& session_options = GetOrtSessionOptionsGpu(num_threads, false);
    try {
      if (m_session_ptr == nullptr) {
        m_session_ptr = std::make_unique<Ort::Session>(util::onnx::GetOrtEnv(), model_path.c_str(), session_options);
      }
    } catch (...) {
      throw OnnxValidationException("Model reading error. File does not exist!");
    }
    m_memory_info_ptr = const_cast<Ort::MemoryInfo*>(&GetMemoryInfoGpu());
  }
}

void BaseOnnxRunner::ValidateModel(const std::vector<std::string>& input_names,
                                   const std::vector<std::string>& output_names) {
  if (!m_session_ptr) {
    std::stringstream iss;
    iss << "The model has not defined session or allocator";
    throw OnnxValidationException(iss.str());
  }

  // Inputs
  const size_t inputs_count = m_session_ptr->GetInputCount();
  if (inputs_count != input_names.size()) {
    std::stringstream iss;
    iss << "The model has " << inputs_count << " inputs, but you provided " << input_names.size();
    throw OnnxValidationException(iss.str());
  }

  bool inputs_names_same = true;
  for (size_t i = 0; i < inputs_count; ++i) {
    m_model_input_names.push_back(m_session_ptr->GetInputNameAllocated(i, m_default_allocator).release());
    m_model_input_shapes.push_back(m_session_ptr->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
    const std::string cur_model_input_name = m_model_input_names.back();
    if (input_names[i] != cur_model_input_name) {
      inputs_names_same = false;
    }
  }
  if (!inputs_names_same) {
    std::stringstream iss;
    iss << "The model has inputs inconsisted with provided ones";
    throw OnnxValidationException(iss.str());
  }

  // Outputs
  const size_t outputs_count = m_session_ptr->GetOutputCount();
  if (outputs_count != output_names.size()) {
    std::stringstream iss;
    iss << "The model has " << outputs_count << " outputs, but you provided " << output_names.size();
    throw OnnxValidationException(iss.str());
  }

  bool outputs_names_same = true;
  for (size_t i = 0; i < outputs_count; ++i) {
    m_model_output_names.push_back(m_session_ptr->GetOutputNameAllocated(i, m_default_allocator).release());
    m_model_output_shapes.push_back(m_session_ptr->GetOutputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
    const std::string cur_model_output_name = m_model_output_names.back();
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

void BaseOnnxRunner::ResizeIfNeededInPlace(std::vector<float>& to_check,
                                           std::vector<int64_t>& shape,
                                           uint32_t num_parts) {
  if (to_check.size() == 0) {
    uint64_t multi = std::accumulate(shape.begin(), shape.end(), 1,
                                     std::multiplies<uint64_t>());
    to_check.resize(multi / num_parts, 0.0);
  }
}

void BaseOnnxRunner::ResizeIfNeededInPlace(std::valarray<float>& to_check,
                                           std::vector<int64_t>& shape,
                                           uint32_t num_parts) {
  if (to_check.size() == 0) {
    uint64_t multi = std::accumulate(shape.begin(), shape.end(), 1,
                                     std::multiplies<uint64_t>());
    to_check.resize(multi / num_parts, 0.0);
  }
}

}  // namespace onnx
}  // namespace util

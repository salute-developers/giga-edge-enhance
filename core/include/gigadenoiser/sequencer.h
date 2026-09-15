/**
 * \file
 * \defgroup group_cpp C++ API
 */
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "gigadenoiser/errors.h"
#include "gigadenoiser/export.h"
#include "gigadenoiser/stream_stat.h"

namespace gigaedge::denoiser {
template <typename T>
struct Context;

class GDN_API Sequencer {
 public:
  /// Parse & validate a YAML document from a raw string
  explicit Sequencer(const std::string& yaml_str) {}
  Sequencer() = default;
  Sequencer(Sequencer&) = delete;
  Sequencer(Sequencer&&) = delete;
  Sequencer& operator=(const Sequencer&) = delete;
  virtual ~Sequencer() = default;

  virtual GigaDenoiserRetCode ProcessChunk(const std::vector<float>& chunk, std::vector<float>& ret,
                                           Context<float>* ctx = nullptr) = 0;

  virtual GigaDenoiserRetCode ProcessChunk(const std::vector<int16_t>& chunk16, std::vector<int16_t>& ret16,
                                           Context<float>* ctx = nullptr) = 0;

  virtual std::vector<float> operator()(const std::vector<float>& chunk, std::vector<float>& ret,
                                        Context<float>* ctx = nullptr) = 0;

  virtual std::vector<int16_t> operator()(const std::vector<int16_t>& chunk16, std::vector<int16_t>& ret16,
                                          Context<float>* ctx = nullptr) = 0;

  virtual GigaDenoiserRetCode GetFinalChunk(std::vector<float>& ret, Context<float>* ctx = nullptr) = 0;

  virtual GigaDenoiserRetCode GetFinalChunk(std::vector<int16_t>& ret16, Context<float>* ctx = nullptr) = 0;

  virtual std::vector<float> operator()(std::vector<float>& ret, Context<float>* ctx = nullptr) = 0;

  virtual std::vector<int16_t> operator()(std::vector<int16_t>& ret16, Context<float>* ctx = nullptr) = 0;
};
}  // namespace gigaedge::denoiser

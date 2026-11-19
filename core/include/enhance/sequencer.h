/**
 * \file
 * \defgroup group_cpp C++ API
 */
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "enhance/errors.h"
#include "enhance/export.h"
#include "enhance/stream_stat.h"

namespace gigaedge::enhance {
template <typename T>
struct Context;

class GEE_API Sequencer {
 public:
  /// Parse & validate a YAML document from a raw string
  explicit Sequencer(const std::string& yaml_str) {}
  Sequencer() = default;
  Sequencer(Sequencer&) = delete;
  Sequencer(Sequencer&&) = delete;
  Sequencer& operator=(const Sequencer&) = delete;
  virtual ~Sequencer() = default;

  virtual GigaEdgeEnhanceRetCode ProcessChunk(const std::vector<float>& chunk, std::vector<float>& ret,
                                              Context<float>* ctx = nullptr) = 0;

  virtual GigaEdgeEnhanceRetCode ProcessChunk(const std::vector<int16_t>& chunk16, std::vector<int16_t>& ret16,
                                              Context<float>* ctx = nullptr) = 0;

  virtual std::vector<float> operator()(const std::vector<float>& chunk, std::vector<float>& ret,
                                        Context<float>* ctx = nullptr) = 0;

  virtual std::vector<int16_t> operator()(const std::vector<int16_t>& chunk16, std::vector<int16_t>& ret16,
                                          Context<float>* ctx = nullptr) = 0;

  virtual GigaEdgeEnhanceRetCode GetFinalChunk(std::vector<float>& ret, Context<float>* ctx = nullptr) = 0;

  virtual GigaEdgeEnhanceRetCode GetFinalChunk(std::vector<int16_t>& ret16, Context<float>* ctx = nullptr) = 0;

  virtual std::vector<float> operator()(std::vector<float>& ret, Context<float>* ctx = nullptr) = 0;

  virtual std::vector<int16_t> operator()(std::vector<int16_t>& ret16, Context<float>* ctx = nullptr) = 0;
};
}  // namespace gigaedge::enhance

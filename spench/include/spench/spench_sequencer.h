/**
 * \file
 * \defgroup group_cpp C++ API
 */
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "spench/errors.h"
#include "spench/export.h"
#include "spench/stream_stat.h"

namespace spench {
template <typename T>
struct SpenchContext;

class SPENCH_API ISpenchSequencer {
 public:
  /// Parse & validate a YAML document from a raw string
  explicit ISpenchSequencer(const std::string& yaml_str) {}
  ISpenchSequencer() = default;
  ISpenchSequencer(ISpenchSequencer&) = delete;
  ISpenchSequencer(ISpenchSequencer&&) = delete;
  ISpenchSequencer& operator=(const ISpenchSequencer&) = delete;
  virtual ~ISpenchSequencer() = default;

  virtual SpenchRetCode ProcessChunk(const std::vector<float>& chunk, std::vector<float>& ret,
                                     SpenchContext<float>* ctx = nullptr) = 0;

  virtual SpenchRetCode ProcessChunk(const std::vector<int16_t>& chunk16, std::vector<int16_t>& ret16,
                                     SpenchContext<float>* ctx = nullptr) = 0;

  virtual std::vector<float> operator()(const std::vector<float>& chunk, std::vector<float>& ret,
                                        SpenchContext<float>* ctx = nullptr) = 0;

  virtual std::vector<int16_t> operator()(const std::vector<int16_t>& chunk16, std::vector<int16_t>& ret16,
                                          SpenchContext<float>* ctx = nullptr) = 0;

  virtual SpenchRetCode GetFinalChunk(std::vector<float>& ret, SpenchContext<float>* ctx = nullptr) = 0;

  virtual SpenchRetCode GetFinalChunk(std::vector<int16_t>& ret16, SpenchContext<float>* ctx = nullptr) = 0;

  virtual std::vector<float> operator()(std::vector<float>& ret, SpenchContext<float>* ctx = nullptr) = 0;

  virtual std::vector<int16_t> operator()(std::vector<int16_t>& ret16, SpenchContext<float>* ctx = nullptr) = 0;
};
}  // namespace spench

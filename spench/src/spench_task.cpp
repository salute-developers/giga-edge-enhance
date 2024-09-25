#include "spench/src/spench_task.h"

namespace spench {

#if defined(_MSC_VER)
// msvc-specific workaround. shamelessly stolen from stackoverflow
#define localtime_r(T, Tm) (localtime_s(Tm, T) ? NULL : Tm)
#endif

std::string GetNowDate() {
  time_t now = time(nullptr);
  struct tm tmbuf;
  struct tm* tstruct;
  char buf[80];
  tstruct = localtime_r(&now, &tmbuf);

  strftime(buf, sizeof(buf), "%Y%m%d", tstruct);
  return buf;
}

std::string GetNowTime() {
  time_t now = time(nullptr);
  struct tm tmbuf;
  struct tm* tstruct;
  char buf[80];
  tstruct = localtime_r(&now, &tmbuf);

  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", tstruct);
  return buf;
}

namespace tools {
/**
 * \brief: Convert float32 to PCM16.
 * \param: A const vector of input float32 data.
 * \param: A vector of output PCM16 data.
 * \return: None
 * \ingroup group_cpp
*/
#ifdef _MSC_VER
__declspec(dllexport)
#endif
void convertFloat32ToPCM16(const std::vector<float>& input, std::vector<int16_t>& output) {
  output.clear();
  for (std::vector<float>::size_type i = 0; i != input.size(); i++) {
    float f = input[i] > 0 ?
          std::round(input[i] * std::numeric_limits<int16_t>::max()):
          std::round(input[i] * -std::numeric_limits<int16_t>::min());
    output.emplace_back(std::clamp(static_cast<int16_t>(f),
                                std::numeric_limits<int16_t>::min(),
                                std::numeric_limits<int16_t>::max()));
  }
}

/**
 * \brief: Convert PCM16 to float32.
 * \param: A const vector of input PCM16 data.
 * \param: A vector of output float32 data.
 * \return: None
 * \ingroup group_cpp
*/
#ifdef _MSC_VER
__declspec(dllexport)
#endif
void convertPCM16ToFloat32(const std::vector<int16_t>& input, std::vector<float>& output) {
  output.clear();
  for (uint16_t i = 0; i != input.size(); i++) {
    float f = input[i] > 0 ?
          static_cast<float>(input[i]) / std::numeric_limits<int16_t>::max():
          static_cast<float>(input[i]) / -std::numeric_limits<int16_t>::min();
    output.emplace_back(f);
  }
}
}  // namespace tools

}  // namespace spench

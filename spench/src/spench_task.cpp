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

}  // namespace spench

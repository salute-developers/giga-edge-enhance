#include <spench/spench_c.h>

int main() {
  SpenchDenoise model = reinterpret_cast<SpenchDenoise>(1);
  if (SpenchDenoiseInit(nullptr, &model) != SpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT || model != nullptr) {
    return 1;
  }
  return 0;
}

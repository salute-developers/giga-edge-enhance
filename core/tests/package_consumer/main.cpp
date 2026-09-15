#include <gigadenoiser/c_api.h>

int main() {
  GigaDenoiserDenoise model = reinterpret_cast<GigaDenoiserDenoise>(1);
  if (GigaDenoiserDenoiseInit(nullptr, &model) != GigaDenoiserRetCode::ERR_INVALID_API_PASSED_ARGUMENT ||
      model != nullptr) {
    return 1;
  }
  return 0;
}

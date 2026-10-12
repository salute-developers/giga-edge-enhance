#include <spench/c_api.h>

int main() {
  GigaEdgeSpenchDenoise model = reinterpret_cast<GigaEdgeSpenchDenoise>(1);
  if (GigaEdgeSpenchDenoiseInit(nullptr, &model) != GigaEdgeSpenchRetCode::ERR_INVALID_API_PASSED_ARGUMENT ||
      model != nullptr) {
    return 1;
  }
  return 0;
}

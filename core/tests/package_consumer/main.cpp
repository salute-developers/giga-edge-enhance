#include <enhance/c_api.h>

int main() {
  GigaEdgeEnhanceDenoise model = reinterpret_cast<GigaEdgeEnhanceDenoise>(1);
  if (GigaEdgeEnhanceDenoiseInit(nullptr, &model) != GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT ||
      model != nullptr) {
    return 1;
  }
  return 0;
}

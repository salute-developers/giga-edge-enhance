#include <enhance/c_api.h>
#include <giga-edge-enhance.h>

int main() {
  // Exercise the installed umbrella header and exported C++ namespace at link time.
  giga_edge::enhance::AudioProcessor processor;
  GigaEdgeEnhanceDenoise model = reinterpret_cast<GigaEdgeEnhanceDenoise>(1);
  if (GigaEdgeEnhanceDenoiseInit(nullptr, &model) != GigaEdgeEnhanceRetCode::ERR_INVALID_API_PASSED_ARGUMENT ||
      model != nullptr) {
    return 1;
  }
  return 0;
}

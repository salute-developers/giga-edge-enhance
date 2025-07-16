#include "spench/src/denoiser_model_adviser.h"

namespace spench {

// Explicit template instantiations to keep build times short and allow the
// implementation to live primarily in the header for other types.
template class DenoiserModelAdviser<float>;

}  // namespace spench

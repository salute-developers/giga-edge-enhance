#include <pocketfft_hdronly.h>

#include <cstdint>
#include <cstring>

struct SpxFftPlan {
  explicit SpxFftPlan(uint32_t n) : nfft{n} { tmp.resize(n * 2); }

  const uint32_t nfft;
  std::vector<float> tmp;
};

// Speex C API uses int; do not change the signature.
extern "C" void* spx_fft_init(int);
extern "C" void spx_fft_destroy(void*);
extern "C" void spx_fft(void*, float*, float*);
extern "C" void spx_ifft(void*, float*, float*);

void* spx_fft_init(int size) {
  auto plan = new SpxFftPlan(size);
  return reinterpret_cast<void*>(plan);
}

void spx_fft_destroy(void* table) {
  auto plan = reinterpret_cast<SpxFftPlan*>(table);
  delete plan;
}

void spx_fft(void* table, float* in, float* out) {
  auto plan = reinterpret_cast<SpxFftPlan*>(table);
  const uint32_t n = plan->nfft;

  auto* tout = reinterpret_cast<std::complex<float>*>(plan->tmp.data());

  constexpr std::ptrdiff_t sz = sizeof(float);
  const pocketfft::shape_t shape{1, n};
  const pocketfft::stride_t in_stride{static_cast<int32_t>(sz * n), sz};
  const pocketfft::stride_t out_stride{static_cast<int32_t>(sz * n * 2), sz * 2};
  pocketfft::r2c<float>(shape, in_stride, out_stride, 1, pocketfft::FORWARD, in, tout, 1.0 / n);

  // ugly speex fft format: re[0], re[1], im[1], re[2], im[2] ...
  out[0] = plan->tmp[0];
  for (size_t i = 1; i < n; ++i) {
    out[i] = plan->tmp[i + 1];
  }
}

void spx_ifft(void* table, float* in, float* out) {
  auto plan = reinterpret_cast<SpxFftPlan*>(table);
  const uint32_t n = plan->nfft;

  auto* tin = plan->tmp.data();

  // speex fft format magic
  tin[0] = in[0];
  tin[1] = 0.0f;
  for (size_t i = 1; i < n; ++i) {
    tin[i + 1] = in[i];
    tin[n + 1] = 0.0f;
  }

  constexpr std::ptrdiff_t sz = sizeof(float);
  const pocketfft::shape_t shape{1, n};
  const pocketfft::stride_t in_stride{static_cast<int32_t>(sz * n * 2), sz * 2};
  const pocketfft::stride_t out_stride{static_cast<int32_t>(sz * n), sz};
  pocketfft::c2r<float>(shape, in_stride, out_stride, 1, pocketfft::BACKWARD,
                        reinterpret_cast<std::complex<float>*>(tin), out, 1.0);
}

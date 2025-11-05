#pragma once
// Forward declarations to avoid including <soxr.h> in public headers.
// soxr declares: typedef struct soxr * soxr_t;
// We reproduce the minimal forward-declare here.
#ifdef __cplusplus
extern "C" {
#endif
struct soxr;
typedef struct soxr * soxr_t;
#ifdef __cplusplus
}
#endif

#import "AtomicOptionalDouble.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace {

// `UINT64_MAX` is the bit pattern of one specific quiet-NaN payload. Loud and
// silent NaN doubles produced by normal arithmetic do not collide with this
// value, but storing such a NaN through `storeValue:` would round-trip as
// `nil`. GigaEdgeEnhance's mean-time computations never produce NaN, so this sentinel
// is safe in practice. Document this limitation if the API is ever extended
// to accept arbitrary doubles.
uint64_t EncodeNil() {
    return UINT64_MAX;
}

uint64_t EncodeDouble(double value) {
    uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

double DecodeDouble(uint64_t bits) {
    double value = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

}  // namespace

@interface AtomicOptionalDouble () {
    std::atomic<uint64_t> _storage;
}
@end

@implementation AtomicOptionalDouble

- (instancetype)init {
    self = [super init];
    if (self == nil) {
        return nil;
    }

    _storage.store(EncodeNil(), std::memory_order_relaxed);
    return self;
}

- (void)storeNil {
    _storage.store(EncodeNil(), std::memory_order_release);
}

- (void)storeValue:(double)value {
    _storage.store(EncodeDouble(value), std::memory_order_release);
}

- (NSNumber*)loadValue {
    const uint64_t bits = _storage.load(std::memory_order_acquire);
    if (bits == EncodeNil()) {
        return nil;
    }

    return @(DecodeDouble(bits));
}

@end

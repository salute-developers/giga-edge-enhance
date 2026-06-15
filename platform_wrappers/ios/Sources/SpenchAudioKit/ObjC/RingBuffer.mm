#import "RingBuffer.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <vector>

namespace {

// Power-of-two capacity makes wrapping a single mask AND, which keeps the
// render-thread copy free of branches.
uint32_t NextPowerOfTwo(uint32_t value) {
    if (value <= 2) {
        return 2;
    }
    value -= 1;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    return value + 1;
}

} // namespace

@interface RingBuffer () {
    std::vector<float> _storage;
    uint32_t _capacity;
    uint32_t _mask;
    std::atomic<uint32_t> _head;
    std::atomic<uint32_t> _tail;
}
@end

@implementation RingBuffer

- (instancetype)initWithCapacity:(uint32_t)capacity {
    self = [super init];
    if (self == nil) {
        return nil;
    }

    _capacity = NextPowerOfTwo(std::max<uint32_t>(capacity, 2));
    _mask = _capacity - 1;
    _storage.assign(_capacity, 0.0f);
    _head.store(0, std::memory_order_relaxed);
    _tail.store(0, std::memory_order_relaxed);
    return self;
}

- (uint32_t)capacity {
    return _capacity;
}

- (uint32_t)availableToRead {
    const uint32_t head = _head.load(std::memory_order_acquire);
    const uint32_t tail = _tail.load(std::memory_order_acquire);
    return head - tail;
}

- (uint32_t)availableToWrite {
    return _capacity - self.availableToRead;
}

- (uint32_t)push:(const float *)input count:(uint32_t)count {
    if (count == 0 || input == nullptr) {
        return 0;
    }

    const uint32_t head = _head.load(std::memory_order_relaxed);
    const uint32_t tail = _tail.load(std::memory_order_acquire);
    const uint32_t freeSpace = _capacity - (head - tail);
    const uint32_t writeCount = std::min(count, freeSpace);
    if (writeCount == 0) {
        return 0;
    }

    const uint32_t startIndex = head & _mask;
    const uint32_t firstChunk = std::min(writeCount, _capacity - startIndex);
    std::memcpy(_storage.data() + startIndex, input, firstChunk * sizeof(float));
    if (writeCount > firstChunk) {
        std::memcpy(_storage.data(), input + firstChunk, (writeCount - firstChunk) * sizeof(float));
    }

    _head.store(head + writeCount, std::memory_order_release);
    return writeCount;
}

- (uint32_t)pop:(float *)output count:(uint32_t)count {
    if (count == 0 || output == nullptr) {
        return 0;
    }

    const uint32_t tail = _tail.load(std::memory_order_relaxed);
    const uint32_t head = _head.load(std::memory_order_acquire);
    const uint32_t available = head - tail;
    const uint32_t readCount = std::min(count, available);
    if (readCount == 0) {
        return 0;
    }

    const uint32_t startIndex = tail & _mask;
    const uint32_t firstChunk = std::min(readCount, _capacity - startIndex);
    std::memcpy(output, _storage.data() + startIndex, firstChunk * sizeof(float));
    if (readCount > firstChunk) {
        std::memcpy(output + firstChunk, _storage.data(), (readCount - firstChunk) * sizeof(float));
    }

    _tail.store(tail + readCount, std::memory_order_release);
    return readCount;
}

- (void)reset {
    _tail.store(_head.load(std::memory_order_acquire), std::memory_order_release);
}

@end

#import "LowPerformanceEventMailbox.h"

#include <atomic>
#include <cstdint>

namespace {

enum class MailboxState : uint32_t {
    empty,
    writing,
    ready,
    reading,
};

}  // namespace

@interface LowPerformanceEventMailbox () {
    std::atomic<MailboxState> _state;
    double _sampleRate;
    double _currentLatency;
    double _meanProcessingMilliseconds;
    uint64_t _refillCount;
}
@end

@implementation LowPerformanceEventMailbox

- (instancetype)init {
    self = [super init];
    if (self == nil) {
        return nil;
    }

    _state.store(MailboxState::empty, std::memory_order_relaxed);
    return self;
}

- (BOOL)storeSampleRate:(double)sampleRate
                currentLatency:(double)currentLatency
    meanProcessingMilliseconds:(double)meanProcessingMilliseconds
                   refillCount:(uint64_t)refillCount {
    MailboxState expected = MailboxState::empty;
    if (!_state.compare_exchange_strong(expected, MailboxState::writing, std::memory_order_acquire)) {
        return NO;
    }

    _sampleRate = sampleRate;
    _currentLatency = currentLatency;
    _meanProcessingMilliseconds = meanProcessingMilliseconds;
    _refillCount = refillCount;
    _state.store(MailboxState::ready, std::memory_order_release);
    return YES;
}

- (BOOL)takeSampleRate:(double*)sampleRate
                currentLatency:(double*)currentLatency
    meanProcessingMilliseconds:(double*)meanProcessingMilliseconds
                   refillCount:(uint64_t*)refillCount {
    MailboxState expected = MailboxState::ready;
    if (!_state.compare_exchange_strong(expected, MailboxState::reading, std::memory_order_acquire)) {
        return NO;
    }

    *sampleRate = _sampleRate;
    *currentLatency = _currentLatency;
    *meanProcessingMilliseconds = _meanProcessingMilliseconds;
    *refillCount = _refillCount;
    _state.store(MailboxState::empty, std::memory_order_release);
    return YES;
}

- (void)reset {
    _state.store(MailboxState::empty, std::memory_order_release);
}

@end

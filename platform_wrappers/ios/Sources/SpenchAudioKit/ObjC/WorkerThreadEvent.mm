#import "WorkerThreadEvent.h"

#include <atomic>

@interface WorkerThreadEvent () {
    dispatch_semaphore_t _semaphore;
    std::atomic<bool> _notified;
    std::atomic<bool> _stopped;
}
@end

@implementation WorkerThreadEvent

// Coalesces repeated wakeups so the worker only processes when new input arrives.

- (instancetype)init {
    self = [super init];
    if (self == nil) {
        return nil;
    }

    _semaphore = dispatch_semaphore_create(0);
    _notified.store(false, std::memory_order_relaxed);
    _stopped.store(false, std::memory_order_relaxed);
    return self;
}

- (void)notify {
    if (_stopped.load(std::memory_order_acquire)) {
        return;
    }

    const bool wasNotified = _notified.exchange(true, std::memory_order_acq_rel);
    if (!wasNotified) {
        dispatch_semaphore_signal(_semaphore);
    }
}

- (BOOL)waitForNotification {
    while (true) {
        if (_stopped.load(std::memory_order_acquire)) {
            return NO;
        }

        const bool hadNotification = _notified.exchange(false, std::memory_order_acq_rel);
        if (hadNotification) {
            return YES;
        }

        dispatch_semaphore_wait(_semaphore, DISPATCH_TIME_FOREVER);
    }
}

- (void)stop {
    _stopped.store(true, std::memory_order_release);
    dispatch_semaphore_signal(_semaphore);
}

@end

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface LowPerformanceEventMailbox : NSObject

- (BOOL)storeSampleRate:(double)sampleRate
                currentLatency:(double)currentLatency
    meanProcessingMilliseconds:(double)meanProcessingMilliseconds
                   refillCount:(uint64_t)refillCount;
- (BOOL)takeSampleRate:(double*)sampleRate
                currentLatency:(double*)currentLatency
    meanProcessingMilliseconds:(double*)meanProcessingMilliseconds
                   refillCount:(uint64_t*)refillCount;
- (void)reset;

@end

NS_ASSUME_NONNULL_END

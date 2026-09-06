#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface RingBuffer : NSObject

- (instancetype)initWithCapacity:(uint32_t)capacity NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, readonly) uint32_t capacity;
@property(nonatomic, readonly) uint32_t availableToRead;
@property(nonatomic, readonly) uint32_t availableToWrite;

- (uint32_t)push:(const float*)input count:(uint32_t)count;
- (uint32_t)pop:(float*)output count:(uint32_t)count;
- (void)reset;

@end

NS_ASSUME_NONNULL_END

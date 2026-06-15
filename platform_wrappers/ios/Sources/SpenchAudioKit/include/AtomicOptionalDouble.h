#ifdef __OBJC__

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface AtomicOptionalDouble : NSObject

- (void)storeNil;
- (void)storeValue:(double)value;
- (nullable NSNumber *)loadValue;

@end

NS_ASSUME_NONNULL_END

#endif

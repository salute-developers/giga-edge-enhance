#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface WorkerThreadEvent : NSObject

- (void)notify;
- (BOOL)waitForNotification;
- (void)stop;

@end

NS_ASSUME_NONNULL_END

#include <stdint.h>

#ifdef __OBJC__

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

extern NSErrorDomain const SpenchErrorDomain;

typedef NS_ERROR_ENUM(SpenchErrorDomain, SpenchErrorCode) {
    SpenchErrorCodeInvalidArgument = 1,
    SpenchErrorCodeOutOfRange = 2,
    SpenchErrorCodeUnknown = 999,
};

@interface PipelineElement : NSObject

- (instancetype)init NS_UNAVAILABLE;

- (uint32_t)processInput:(const float *)input
                  output:(float *)output
              frameCount:(uint32_t)frameCount;

- (void)resetContext;

@end

@interface AudioProcessPipelineElement : PipelineElement

- (nullable instancetype)initWithYAMLConfig:(NSString *)yamlConfig
                                  sampleRate:(float)sampleRate
                                       error:(NSError **)error NS_DESIGNATED_INITIALIZER;

- (float)sampleRate;

@end

@interface DenoisePipelineElement : PipelineElement

- (nullable instancetype)initWithModelPath:(NSString *)modelPath
                                  sampleRate:(float)sampleRate
                                        error:(NSError **)error NS_DESIGNATED_INITIALIZER;

- (float)sampleRate;

@end

NS_ASSUME_NONNULL_END

#endif

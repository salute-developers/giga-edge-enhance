#import "PipelineElements.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#import "spench/packed_model.h"
#import "spench/spench_audio_denoise.h"
#import "spench/spench_audio_process.h"

NSErrorDomain const SpenchErrorDomain = @"com.gigaedge.SpenchAudioKit.SpenchError";

namespace spench_ios {

// Matches SpenchAUAudioUnit.maximumFramesToRender so render calls do not grow
// the wrapper's scratch vectors.
constexpr size_t kMaximumRenderFrames = 4096;

class PipelineElementCore {
   protected:
    uint32_t sampleRate_ = 0;
    std::vector<float> input_;
    std::vector<float> output_;

   public:
    explicit PipelineElementCore(uint32_t sampleRate) noexcept : sampleRate_(sampleRate) {
        input_.reserve(kMaximumRenderFrames);
        output_.reserve(kMaximumRenderFrames);
    }
    virtual ~PipelineElementCore() = default;

    PipelineElementCore() = delete;
    PipelineElementCore(const PipelineElementCore&) = delete;
    PipelineElementCore& operator=(const PipelineElementCore&) = delete;

    virtual uint32_t process(const float* __restrict src,
                             float* __restrict dst,
                             uint32_t frameCount) noexcept = 0;
    virtual void resetContext() noexcept = 0;

    uint32_t sampleRate() const noexcept {
        return sampleRate_;
    }
};

class AudioProcessPipelineElementCore final : public PipelineElementCore {
   private:
    std::string yamlConfig_;
    std::unique_ptr<spench::SpenchAudioProcess> processor_;

   public:
    AudioProcessPipelineElementCore(const std::string& yamlConfig, uint32_t sampleRate)
        : PipelineElementCore(sampleRate),
          yamlConfig_(yamlConfig),
          processor_(std::make_unique<spench::SpenchAudioProcess>(yamlConfig)) {
        processor_->AddLC().AddNG().AddLim();
    }

    uint32_t process(const float* __restrict src, float* __restrict dst, uint32_t frameCount) noexcept final {
        try {
            input_.assign(src, src + frameCount);
            output_.clear();
            if (processor_->ProcessChunk(input_, output_) < SUCCESS || output_.size() > frameCount) {
                output_.clear();
                return 0;
            }
            if (!output_.empty()) {
                std::memcpy(dst, output_.data(), output_.size() * sizeof(float));
            }
            return static_cast<uint32_t>(output_.size());
        } catch (...) {
            output_.clear();
            return 0;
        }
    }

    void resetContext() noexcept final {
        try {
            auto processor = std::make_unique<spench::SpenchAudioProcess>(yamlConfig_);
            processor->AddLC().AddNG().AddLim();
            processor_ = std::move(processor);
            input_.clear();
            output_.clear();
        } catch (...) {}
    }
};

class DenoisePipelineElementCore final : public PipelineElementCore {
   private:
    spench::SpenchDenoise denoiser_;
    spench::SpenchContext<float>* context_ = nullptr;
    std::array<char, 256> lastProcessingError_{};

    void setProcessingError(const char* message) noexcept {
        std::snprintf(lastProcessingError_.data(),
                      lastProcessingError_.size(),
                      "%s",
                      message != nullptr ? message : "Unknown denoising error.");
    }

   public:
    DenoisePipelineElementCore(const std::string& modelPath, uint32_t sampleRate)
        : PipelineElementCore(sampleRate), denoiser_(std::make_shared<spench::PackedModelReader>(modelPath)) {
        context_ = denoiser_.CreateContext(sampleRate_, sampleRate_);
    }

    ~DenoisePipelineElementCore() override {
        try {
            if (context_ != nullptr) {
                denoiser_.DestroyContext(&context_);
            }
        } catch (...) {}
    }

    uint32_t process(const float* __restrict src, float* __restrict dst, uint32_t frameCount) noexcept final {
        lastProcessingError_[0] = '\0';
        try {
            input_.assign(src, src + frameCount);
            output_.clear();

            const SpenchRetCode retCode = denoiser_.ProcessChunk(input_, output_, context_);
            if (retCode < SUCCESS) {
                std::snprintf(lastProcessingError_.data(),
                              lastProcessingError_.size(),
                              "Denoising failed with native error code %d.",
                              static_cast<int32_t>(retCode));
                return 0;
            }

            if (output_.size() > frameCount) {
                setProcessingError(
                    "Denoising produced more samples than the equal-rate output buffer can hold.");
                output_.clear();
                return 0;
            }

            if (!output_.empty()) {
                std::memcpy(dst, output_.data(), output_.size() * sizeof(float));
            }

            return static_cast<uint32_t>(output_.size());
        } catch (const std::exception& exception) {
            setProcessingError(exception.what());
            output_.clear();
            return 0;
        } catch (...) {
            setProcessingError("Unknown denoising error.");
            output_.clear();
            return 0;
        }
    }

    void resetContext() noexcept final {
        lastProcessingError_[0] = '\0';
        try {
            if (context_ != nullptr) {
                denoiser_.DestroyContext(&context_);
            }
            context_ = denoiser_.CreateContext(sampleRate_, sampleRate_);
            input_.clear();
            output_.clear();
        } catch (const std::exception& exception) {
            context_ = nullptr;
            setProcessingError(exception.what());
        } catch (...) {
            context_ = nullptr;
            setProcessingError("Unknown denoising reset error.");
        }
    }

    const char* lastProcessingError() const noexcept {
        return lastProcessingError_.data();
    }
};

void ValidateSampleRate(float sampleRate) {
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0F) {
        throw std::invalid_argument("sampleRate must be greater than zero");
    }
}

NSError* SpenchErrorFromException(const std::exception& exception) {
    SpenchErrorCode code = SpenchErrorCodeUnknown;
    if (dynamic_cast<const std::invalid_argument*>(&exception) != nullptr) {
        code = SpenchErrorCodeInvalidArgument;
    } else if (dynamic_cast<const std::out_of_range*>(&exception) != nullptr) {
        code = SpenchErrorCodeOutOfRange;
    }

    NSDictionary* userInfo = @{NSLocalizedDescriptionKey : [NSString stringWithUTF8String:exception.what()]};
    return [NSError errorWithDomain:SpenchErrorDomain code:code userInfo:userInfo];
}

}  // namespace spench_ios

@interface PipelineElement () {
   @protected
    std::unique_ptr<spench_ios::PipelineElementCore> _core;
}

- (nullable instancetype)initWithCore:(std::unique_ptr<spench_ios::PipelineElementCore>)core;

@end

@implementation PipelineElement

- (nullable instancetype)initWithCore:(std::unique_ptr<spench_ios::PipelineElementCore>)core {
    self = [super init];
    if (self == nil) {
        return nil;
    }

    _core = std::move(core);
    return self;
}

- (uint32_t)processInput:(const float*)input output:(float*)output frameCount:(uint32_t)frameCount {
    if (input == nullptr || output == nullptr || _core == nullptr) {
        return 0;
    }

    return _core->process(input, output, frameCount);
}

- (void)resetContext {
    if (_core != nullptr) {
        _core->resetContext();
    }
}

@end

@implementation AudioProcessPipelineElement

- (nullable instancetype)initWithYAMLConfig:(NSString*)yamlConfig
                                 sampleRate:(float)sampleRate
                                      error:(NSError* _Nullable __autoreleasing*)error {
    try {
        spench_ios::ValidateSampleRate(sampleRate);
        std::string yaml([yamlConfig UTF8String]);
        auto core = std::make_unique<spench_ios::AudioProcessPipelineElementCore>(
            yaml, static_cast<uint32_t>(std::lround(sampleRate)));
        return [super initWithCore:std::move(core)];
    } catch (const std::exception& exception) {
        if (error != nil) {
            *error = spench_ios::SpenchErrorFromException(exception);
        }
        return nil;
    }
}

- (float)sampleRate {
    return _core == nullptr ? 0.0F : static_cast<float>(_core->sampleRate());
}

@end

@implementation DenoisePipelineElement

- (nullable NSString*)lastProcessingError {
    const auto* core = dynamic_cast<const spench_ios::DenoisePipelineElementCore*>(_core.get());
    if (core == nullptr || core->lastProcessingError()[0] == '\0') {
        return nil;
    }
    return [NSString stringWithUTF8String:core->lastProcessingError()];
}

- (nullable instancetype)initWithModelPath:(NSString*)modelPath
                                sampleRate:(float)sampleRate
                                     error:(NSError* _Nullable __autoreleasing*)error {
    try {
        spench_ios::ValidateSampleRate(sampleRate);
        if (modelPath.length == 0) {
            throw std::invalid_argument("modelPath must not be empty");
        }

        auto core = std::make_unique<spench_ios::DenoisePipelineElementCore>(
            std::string(modelPath.UTF8String), static_cast<uint32_t>(std::lround(sampleRate)));
        return [super initWithCore:std::move(core)];
    } catch (const std::exception& exception) {
        if (error != nil) {
            *error = spench_ios::SpenchErrorFromException(exception);
        }
        return nil;
    }
}

- (float)sampleRate {
    return _core == nullptr ? 0.0F : static_cast<float>(_core->sampleRate());
}

@end

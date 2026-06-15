#import "PipelineElements.h"

#include <cmath>
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

// Encryption token must match the model build configuration. The C++ core is
// built with `BUILD_WITHOUT_MODEL_ENCRYPTION=OFF` (see scripts/build_core.sh),
// so the bundled .spench models are encrypted with this token.
constexpr char kModelEncryptionToken[] = "REDACTED";

// Reused chunk size for `reserve`; matches a typical render-buffer size at
// 48 kHz/16 ms and lets the first calls avoid allocations.
constexpr size_t kInitialScratchCapacity = 1024;

class PipelineElementCore {
 protected:
    uint32_t sampleRate_ = 0;
    std::vector<float> input_;
    std::vector<float> output_;

 public:
    explicit PipelineElementCore(uint32_t sampleRate) noexcept : sampleRate_(sampleRate) {
        input_.reserve(kInitialScratchCapacity);
        output_.reserve(kInitialScratchCapacity);
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
    spench::SpenchAudioProcess processor_;

 public:
    AudioProcessPipelineElementCore(const std::string& yamlConfig, uint32_t sampleRate)
        : PipelineElementCore(sampleRate), processor_(yamlConfig) {
        processor_.AddLC().AddNG().AddLim();
    }

    uint32_t process(const float* __restrict src,
                     float* __restrict dst,
                     uint32_t frameCount) noexcept final {
        input_.assign(src, src + frameCount);
        output_.resize(frameCount);
        processor_.ProcessChunk(input_, output_);

        if (!output_.empty()) {
            std::memcpy(dst, output_.data(), output_.size() * sizeof(float));
        }

        return static_cast<uint32_t>(output_.size());
    }

    void resetContext() noexcept final {}
};

class DenoisePipelineElementCore final : public PipelineElementCore {
 private:
    spench::SpenchDenoise denoiser_;
    spench::SpenchContext<float>* context_ = nullptr;

 public:
    DenoisePipelineElementCore(const std::string& modelPath, uint32_t sampleRate)
        : PipelineElementCore(sampleRate),
          denoiser_(std::make_shared<spench::PackedModelReader>(modelPath), kModelEncryptionToken) {
        context_ = denoiser_.CreateContext(sampleRate_, sampleRate_);
    }

    ~DenoisePipelineElementCore() override {
        if (context_ != nullptr) {
            denoiser_.DestroyContext(&context_);
        }
    }

    uint32_t process(const float* __restrict src,
                     float* __restrict dst,
                     uint32_t frameCount) noexcept final {
        input_.assign(src, src + frameCount);

        const SpenchRetCode retCode = denoiser_.ProcessChunk(input_, output_, context_);
        if (retCode < SUCCESS) {
            return 0;
        }

        if (!output_.empty()) {
            std::memcpy(dst, output_.data(), output_.size() * sizeof(float));
        }

        return static_cast<uint32_t>(output_.size());
    }

    void resetContext() noexcept final {
        if (context_ != nullptr) {
            denoiser_.DestroyContext(&context_);
        }
        context_ = denoiser_.CreateContext(sampleRate_, sampleRate_);
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

    NSDictionary* userInfo = @{NSLocalizedDescriptionKey: [NSString stringWithUTF8String:exception.what()]};
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
        auto core = std::make_unique<spench_ios::AudioProcessPipelineElementCore>(yaml,
                                                                                  static_cast<uint32_t>(std::lround(sampleRate)));
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

- (nullable instancetype)initWithModelPath:(NSString*)modelPath
                                  sampleRate:(float)sampleRate
                                        error:(NSError* _Nullable __autoreleasing*)error {
    try {
        spench_ios::ValidateSampleRate(sampleRate);
        if (modelPath.length == 0) {
            throw std::invalid_argument("modelPath must not be empty");
        }

        auto core = std::make_unique<spench_ios::DenoisePipelineElementCore>(std::string(modelPath.UTF8String),
                                                                             static_cast<uint32_t>(std::lround(sampleRate)));
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

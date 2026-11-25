#include <emscripten/bind.h>

#include "core/src/logger.h"
#include "elements.h"
/**
 *    trace = 0,
 *    debug = 1,
 *    info = 2,
 *    warn = 3,
 *    err = 4,
 *    critical = 5,
 *    off = 6,
 *    n_levels
 */

EMSCRIPTEN_BINDINGS(enhance) {
#ifdef BUILD_DSP
  emscripten::class_<AudioProcessPipelineElement>("AudioProcessPipelineElement")
    .constructor<std::string>()
    .function("process", &AudioProcessPipelineElement::process)
    .function("resetContext", &AudioProcessPipelineElement::resetContext);
  emscripten::class_<EchoCancellationElement>("EchoCancelationElement")
    .constructor<std::string>()
    .function("process", &EchoCancellationElement::processMono)
    .function("resetContext", &EchoCancellationElement::resetContext);
#else
  emscripten::class_<giga_edge::enhance::PackedModelReader>("PackedModelReader")
      .smart_ptr_constructor("PackedModelReader",
                             &std::make_shared<giga_edge::enhance::PackedModelReader, const uintptr_t, uint32_t>);
  emscripten::class_<DenoisePipelineElement>("DenoisePipelineElement")
      .constructor<uint32_t, std::shared_ptr<giga_edge::enhance::PackedModelReader> >()
      .function("process", &DenoisePipelineElement::process)
      .function("resetContext", &DenoisePipelineElement::resetContext);
#endif
  emscripten::function("SetLogLevel", &giga_edge::enhance::set_log_level);
  emscripten::function("InitLogger", &giga_edge::enhance::init_logger);
}

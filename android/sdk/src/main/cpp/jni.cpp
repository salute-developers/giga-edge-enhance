#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <jni.h>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "enhance/processor.h"

namespace {

using giga_edge::enhance::Processor;

constexpr const char* illegal_argument_exception = "java/lang/IllegalArgumentException";
constexpr const char* illegal_state_exception = "java/lang/IllegalStateException";
constexpr const char* model_asset_prefix = "enhance/models/";
constexpr const char* model_asset_suffix = ".enhance";

void throw_java_exception(JNIEnv* env, const char* class_name, const char* message) {
  jclass exception_class = env->FindClass(class_name);
  if (exception_class != nullptr) {
    env->ThrowNew(exception_class, message);
  }
}

Processor* from_handle(jlong handle) { return reinterpret_cast<Processor*>(handle); }

Processor* require_processor(JNIEnv* env, jlong handle) {
  Processor* processor = from_handle(handle);
  if (processor == nullptr) {
    throw_java_exception(env, illegal_state_exception, "Processor handle is closed or invalid");
  }
  return processor;
}

std::string get_java_string(JNIEnv* env, jstring value) {
  if (value == nullptr) {
    throw std::invalid_argument("modelName must be non-null");
  }
  const char* chars = env->GetStringUTFChars(value, nullptr);
  if (chars == nullptr) {
    throw std::runtime_error("Failed to read modelName");
  }
  std::string result(chars);
  env->ReleaseStringUTFChars(value, chars);
  return result;
}

std::string get_nullable_java_string(JNIEnv* env, jstring value) {
  if (value == nullptr) {
    return {};
  }
  return get_java_string(env, value);
}

jdoubleArray make_double_array(JNIEnv* env, const std::vector<double>& values) {
  jdoubleArray array = env->NewDoubleArray(static_cast<jsize>(values.size()));
  if (array == nullptr) {
    throw std::runtime_error("Failed to allocate telemetry bucket array");
  }
  env->SetDoubleArrayRegion(array, 0, static_cast<jsize>(values.size()), values.data());
  return array;
}

jintArray make_int_array(JNIEnv* env, const std::vector<jint>& values) {
  jintArray array = env->NewIntArray(static_cast<jsize>(values.size()));
  if (array == nullptr) {
    throw std::runtime_error("Failed to allocate telemetry count array");
  }
  env->SetIntArrayRegion(array, 0, static_cast<jsize>(values.size()), values.data());
  return array;
}

std::vector<std::uint8_t> read_asset(AAssetManager* asset_manager, const std::string& asset_path) {
  AAsset* asset = AAssetManager_open(asset_manager, asset_path.c_str(), AASSET_MODE_BUFFER);
  if (asset == nullptr) {
    throw std::invalid_argument("GigaEdgeEnhance model asset not found: " + asset_path);
  }

  const off_t length = AAsset_getLength(asset);
  if (length <= 0) {
    AAsset_close(asset);
    throw std::runtime_error("GigaEdgeEnhance model asset is empty: " + asset_path);
  }

  std::vector<std::uint8_t> data(static_cast<std::size_t>(length));
  std::size_t total_read = 0U;
  while (total_read < data.size()) {
    const std::int32_t bytes_read = AAsset_read(asset, data.data() + total_read, data.size() - total_read);
    if (bytes_read <= 0) {
      AAsset_close(asset);
      throw std::runtime_error("Failed to read GigaEdgeEnhance model asset: " + asset_path);
    }
    total_read += static_cast<std::size_t>(bytes_read);
  }
  AAsset_close(asset);

  return data;
}

}  // namespace

// JNI exports are part of the Java native-method ABI and cannot use internal naming.
// NOLINTBEGIN(readability-identifier-naming)
extern "C" JNIEXPORT jlong JNICALL Java_com_gigaedge_enhance_Processor_nativeCreate(
    JNIEnv* env, jobject /* thiz */, jint sample_rate_hz, jdouble low_performance_window_seconds,
    jint low_performance_max_refill_times, jdouble low_performance_max_refill_time_factor, jdouble min_latency,
    jdouble latency_increase_step, jdouble telemetry_rounding_milliseconds, jboolean use_vad_normalization,
    jboolean unstable_reverse_processing_order, jstring unstable_custom_normalization_config,
    jdouble max_processing_chunk_seconds) {
  try {
    if (sample_rate_hz <= 0) {
      throw std::invalid_argument("sampleRateHz must be greater than zero");
    }
    giga_edge::enhance::ProcessorSettings settings;
    settings.low_performance_window_seconds = low_performance_window_seconds;
    settings.low_performance_max_refill_times = low_performance_max_refill_times;
    settings.low_performance_max_refill_time_factor = low_performance_max_refill_time_factor;
    settings.min_latency = min_latency;
    settings.latency_increase_step = latency_increase_step;
    settings.max_processing_chunk_seconds = max_processing_chunk_seconds;
    settings.telemetry_rounding_milliseconds = telemetry_rounding_milliseconds;
    settings.use_vad_normalization = use_vad_normalization == JNI_TRUE;
    settings.unstable_reverse_processing_order = unstable_reverse_processing_order == JNI_TRUE;
    settings.unstable_custom_normalization_config = get_nullable_java_string(env, unstable_custom_normalization_config);
    std::unique_ptr<Processor> processor =
        std::make_unique<Processor>(static_cast<std::uint32_t>(sample_rate_hz), std::move(settings));
    return reinterpret_cast<jlong>(processor.release());
  } catch (const std::invalid_argument& exception) {
    throw_java_exception(env, illegal_argument_exception, exception.what());
    return 0;
  } catch (const std::bad_alloc&) {
    throw_java_exception(env, illegal_state_exception, "Failed to allocate processor");
    return 0;
  } catch (const std::exception& exception) {
    throw_java_exception(env, illegal_state_exception, exception.what());
    return 0;
  }
}

extern "C" JNIEXPORT void JNICALL Java_com_gigaedge_enhance_Processor_nativeDestroy(JNIEnv* /* env */,
                                                                                    jobject /* thiz */, jlong handle) {
  delete from_handle(handle);
}

extern "C" JNIEXPORT void JNICALL Java_com_gigaedge_enhance_Processor_nativeProcess(JNIEnv* env, jobject /* thiz */,
                                                                                    jlong handle, jfloatArray input,
                                                                                    jfloatArray output,
                                                                                    jint num_frames) {
  Processor* processor = require_processor(env, handle);
  if (processor == nullptr) {
    return;
  }
  if (input == nullptr || output == nullptr) {
    throw_java_exception(env, illegal_argument_exception, "Input and output buffers must be non-null");
    return;
  }
  const jsize input_length = env->GetArrayLength(input);
  const jsize output_length = env->GetArrayLength(output);
  if (num_frames < 0 || input_length < num_frames || output_length < num_frames) {
    throw_java_exception(env, illegal_argument_exception, "Buffer sizes do not match numFrames");
    return;
  }

  jfloat* input_ptr = env->GetFloatArrayElements(input, nullptr);
  if (input_ptr == nullptr) {
    throw_java_exception(env, illegal_state_exception, "Failed to access input buffer");
    return;
  }
  jfloat* output_ptr = env->GetFloatArrayElements(output, nullptr);
  if (output_ptr == nullptr) {
    env->ReleaseFloatArrayElements(input, input_ptr, JNI_ABORT);
    throw_java_exception(env, illegal_state_exception, "Failed to access output buffer");
    return;
  }

  try {
    processor->process(input_ptr, output_ptr, static_cast<std::size_t>(num_frames));
  } catch (const std::invalid_argument& exception) {
    throw_java_exception(env, illegal_argument_exception, exception.what());
  } catch (const std::exception& exception) {
    throw_java_exception(env, illegal_state_exception, exception.what());
  }

  env->ReleaseFloatArrayElements(input, input_ptr, JNI_ABORT);
  env->ReleaseFloatArrayElements(output, output_ptr, 0);
}

extern "C" JNIEXPORT void JNICALL Java_com_gigaedge_enhance_Processor_nativeReset(JNIEnv* env, jobject /* thiz */,
                                                                                  jlong handle) {
  Processor* processor = require_processor(env, handle);
  if (processor == nullptr) {
    return;
  }
  try {
    processor->reset();
  } catch (const std::invalid_argument& exception) {
    throw_java_exception(env, illegal_argument_exception, exception.what());
  } catch (const std::exception& exception) {
    throw_java_exception(env, illegal_state_exception, exception.what());
  }
}

extern "C" JNIEXPORT jobject JNICALL Java_com_gigaedge_enhance_Processor_nativeGetTelemetry(JNIEnv* env,
                                                                                            jobject /* thiz */,
                                                                                            jlong handle) {
  try {
    Processor* processor = require_processor(env, handle);
    if (processor == nullptr) {
      return nullptr;
    }
    const giga_edge::enhance::TelemetrySnapshot snapshot = processor->telemetry_snapshot();
    std::vector<double> normalization_buckets;
    std::vector<jint> normalization_counts;
    std::vector<double> noise_buckets;
    std::vector<jint> noise_counts;
    for (const auto& entry : snapshot.normalization_counters) {
      normalization_buckets.push_back(static_cast<double>(entry.first) * snapshot.rounding_milliseconds);
      normalization_counts.push_back(entry.second);
    }
    for (const auto& entry : snapshot.noise_suppression_counters) {
      noise_buckets.push_back(static_cast<double>(entry.first) * snapshot.rounding_milliseconds);
      noise_counts.push_back(entry.second);
    }

    jclass clazz = env->FindClass("com/gigaedge/enhance/NativeTelemetry");
    if (clazz == nullptr) {
      return nullptr;  // FindClass left a pending exception (e.g. class stripped by R8).
    }
    jmethodID constructor = env->GetMethodID(clazz, "<init>", "([D[I[D[I)V");
    if (constructor == nullptr) {
      return nullptr;  // GetMethodID left a pending NoSuchMethodError.
    }
    return env->NewObject(clazz, constructor, make_double_array(env, normalization_buckets),
                          make_int_array(env, normalization_counts), make_double_array(env, noise_buckets),
                          make_int_array(env, noise_counts));
  } catch (const std::invalid_argument& exception) {
    throw_java_exception(env, illegal_argument_exception, exception.what());
    return nullptr;
  } catch (const std::exception& exception) {
    throw_java_exception(env, illegal_state_exception, exception.what());
    return nullptr;
  }
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_gigaedge_enhance_Processor_nativeDrainLowPerformanceEvents(JNIEnv* env, jobject /* thiz */, jlong handle) {
  try {
    Processor* processor = require_processor(env, handle);
    if (processor == nullptr) {
      return nullptr;
    }
    const std::vector<giga_edge::enhance::LowPerformanceEventNative> events = processor->drain_low_performance_events();
    jclass event_class = env->FindClass("com/gigaedge/enhance/LowPerformanceEvent");
    if (event_class == nullptr) {
      return nullptr;  // Pending exception; class likely stripped/renamed by R8 without keep rules.
    }
    jmethodID event_constructor = env->GetMethodID(event_class, "<init>", "(IDLjava/lang/Double;I)V");
    if (event_constructor == nullptr) {
      return nullptr;
    }
    jclass double_class = env->FindClass("java/lang/Double");
    if (double_class == nullptr) {
      return nullptr;
    }
    jmethodID double_value_of = env->GetStaticMethodID(double_class, "valueOf", "(D)Ljava/lang/Double;");
    if (double_value_of == nullptr) {
      return nullptr;
    }
    jobjectArray array = env->NewObjectArray(static_cast<jsize>(events.size()), event_class, nullptr);
    if (array == nullptr) {
      throw std::runtime_error("Failed to allocate low-performance event array");
    }
    for (std::size_t i = 0; i < events.size(); ++i) {
      jobject mean = nullptr;
      if (events[i].mean_processing_time_milliseconds >= 0.0) {
        mean = env->CallStaticObjectMethod(double_class, double_value_of, events[i].mean_processing_time_milliseconds);
      }
      jobject event =
          env->NewObject(event_class, event_constructor, static_cast<jint>(events[i].sample_rate_hz),
                         events[i].current_latency_seconds, mean, static_cast<jint>(events[i].refill_count));
      if (env->ExceptionCheck() == JNI_TRUE) {
        return nullptr;  // Propagate the pending exception rather than continuing on a null ref.
      }
      env->SetObjectArrayElement(array, static_cast<jsize>(i), event);
      env->DeleteLocalRef(event);
      if (mean != nullptr) {
        env->DeleteLocalRef(mean);
      }
    }
    return array;
  } catch (const std::invalid_argument& exception) {
    throw_java_exception(env, illegal_argument_exception, exception.what());
    return nullptr;
  } catch (const std::exception& exception) {
    throw_java_exception(env, illegal_state_exception, exception.what());
    return nullptr;
  }
}

extern "C" JNIEXPORT void JNICALL Java_com_gigaedge_enhance_Processor_nativeSetFeatures(JNIEnv* env, jobject /* thiz */,
                                                                                        jlong handle,
                                                                                        jobject asset_manager_object,
                                                                                        jboolean normalization,
                                                                                        jstring noise_suppression) {
  try {
    Processor* processor = require_processor(env, handle);
    if (processor == nullptr) {
      return;
    }

    std::string model_name;
    std::shared_ptr<giga_edge::enhance::PackedModelReader> model_reader;
    if (noise_suppression != nullptr) {
      if (asset_manager_object == nullptr) {
        throw std::invalid_argument("AssetManager must be non-null");
      }
      AAssetManager* asset_manager = AAssetManager_fromJava(env, asset_manager_object);
      if (asset_manager == nullptr) {
        throw std::runtime_error("Failed to access AssetManager");
      }

      model_name = get_java_string(env, noise_suppression);
      const std::string asset_path = std::string(model_asset_prefix) + model_name + model_asset_suffix;
      const std::vector<std::uint8_t> model_data = read_asset(asset_manager, asset_path);
      model_reader = std::make_shared<giga_edge::enhance::PackedModelReader>(model_data);
    }

    processor->set_features(normalization == JNI_TRUE, model_name, model_reader);
  } catch (const std::invalid_argument& exception) {
    throw_java_exception(env, illegal_argument_exception, exception.what());
  } catch (const std::bad_alloc&) {
    throw_java_exception(env, illegal_state_exception, "Failed to allocate processor feature state");
  } catch (const std::exception& exception) {
    throw_java_exception(env, illegal_state_exception, exception.what());
  }
  // NOLINTEND(readability-identifier-naming)
}

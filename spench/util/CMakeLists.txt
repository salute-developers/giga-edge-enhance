cmake_minimum_required(VERSION 3.17.3)

ADD_SUBDIRECTORY(sound)
ADD_SUBDIRECTORY(metrics)
ADD_SUBDIRECTORY(echo)
ADD_SUBDIRECTORY(packed_model)

if (BUILD_TFLITE_ENGINE)
  ADD_SUBDIRECTORY(tflite)
elseif (BUILD_ONNX_ENGINE OR TARGET onnxruntime::onnxruntime)
  ADD_SUBDIRECTORY(onnx)
endif()

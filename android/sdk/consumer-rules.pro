# Consumer ProGuard/R8 rules for the GigaEdgeEnhance SDK.
#
# The native (JNI) layer reaches several Kotlin members reflectively by name/signature:
#   * jni.cpp constructs NativeTelemetry via <init>([D[I[D[I)V
#   * jni.cpp constructs LowPerformanceEvent via <init>(IDLjava/lang/Double;I)V
#   * The nativeXxx methods on Processor are bound by their mangled JNI symbol names.
# Without these keeps, a minified consumer app could strip/rename them and crash at runtime.

-keep class com.gigaedge.enhance.NativeTelemetry { *; }
-keep class com.gigaedge.enhance.LowPerformanceEvent { *; }

-keepclasseswithmembers class com.gigaedge.enhance.Processor {
    native <methods>;
}

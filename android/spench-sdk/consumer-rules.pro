# Consumer ProGuard/R8 rules for the Spench SDK.
#
# The native (JNI) layer reaches several Kotlin members reflectively by name/signature:
#   * spench_jni.cpp constructs SpenchNativeTelemetry via <init>([D[I[D[I)V
#   * spench_jni.cpp constructs SpenchLowPerformanceEvent via <init>(IDLjava/lang/Double;I)V
#   * The nativeXxx methods on SpenchProcessor are bound by their mangled JNI symbol names.
# Without these keeps, a minified consumer app could strip/rename them and crash at runtime.

-keep class com.gigaedge.spench.SpenchNativeTelemetry { *; }
-keep class com.gigaedge.spench.SpenchLowPerformanceEvent { *; }

-keepclasseswithmembers class com.gigaedge.spench.SpenchProcessor {
    native <methods>;
}

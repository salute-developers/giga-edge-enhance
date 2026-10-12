# ProGuard / R8

**You do not need to add any manual keep rules for the GigaEdge Spench SDK.** The SDK ships consumer ProGuard
rules that are applied automatically to your app when you depend on `gigaedge-spench-sdk`.

## What ships

`gigaedge-spench-sdk` declares `consumerProguardFiles("consumer-rules.pro")` in its Gradle config, so R8/
ProGuard in the consuming app automatically honors `sdk/consumer-rules.pro`:

```proguard
-keep class com.gigaedge.spench.NativeTelemetry { *; }
-keep class com.gigaedge.spench.LowPerformanceEvent { *; }

-keepclasseswithmembers class com.gigaedge.spench.Processor {
    native <methods>;
}
```

## Why these rules exist

The native (JNI) layer reaches several Kotlin members reflectively, by name and signature:

- `NativeTelemetry` is constructed from native code via its `<init>([D[I[D[I)V` constructor.
- `LowPerformanceEvent` is constructed from native code via its
  `<init>(IDLjava/lang/Double;I)V` constructor.
- The `nativeXxx` methods on `Processor` are bound to their mangled JNI symbol names.

Without these keeps, a **minified** consumer app could rename or strip these members, and the JNI
lookups (`FindClass` / `GetMethodID`) would fail at runtime — for example telemetry or
low-performance events could stop being delivered, or the library could crash. Because the rules are
consumer rules, they take effect in the app's release (minified) build with no action from you.

## Consumer takeaway

- No manual `-keep` rules are required for the SDK.
- Do not remove or override these keeps in your app's ProGuard configuration.
- These rules are minimal and scoped to the JNI-facing types; they do not affect the rest of your
  app's shrinking.

# JNI entry points are resolved by name.
-keepclasseswithmembernames class * {
    native <methods>;
}
-keep class com.danimanx.replicanttr8s.audio.NativeEngine { *; }

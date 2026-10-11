-keep class com.google.zxing.** { *; }
-keep class com.hexadecinull.qrscan.legacy.** { *; }
-keepattributes Signature
-keepattributes *Annotation*
-keepattributes SourceFile,LineNumberTable
-dontwarn com.google.zxing.**
-assumenosideeffects class android.util.Log {
    public static *** d(...);
    public static *** v(...);
}

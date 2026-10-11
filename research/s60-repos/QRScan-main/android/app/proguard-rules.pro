-keep class com.google.zxing.** { *; }
-keep class com.google.mlkit.** { *; }

-keep class com.hexadecinull.qrscan.db.** { *; }
-keep class com.hexadecinull.qrscan.encode.** { *; }
-keep class com.hexadecinull.qrscan.decode.** { *; }

-keepattributes Signature
-keepattributes *Annotation*
-keepattributes SourceFile,LineNumberTable

-dontwarn com.google.zxing.**
-dontwarn org.bouncycastle.**
-dontwarn org.conscrypt.**
-dontwarn org.openjsse.**

-keep class androidx.camera.** { *; }
-keep class androidx.compose.** { *; }

-keepclassmembers class * extends androidx.room.RoomDatabase {
    public static ** getInstance(...);
}

-assumenosideeffects class android.util.Log {
    public static *** d(...);
    public static *** v(...);
    public static *** i(...);
}

# id Tech 3 Engine ProGuard Rules
# Keep native methods and JNI classes
-keep class com.gopex.idtech3.** { *; }
-keepclassmembers class * { native <methods>; }

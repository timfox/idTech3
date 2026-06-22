# SDL2 Android Imports

Place the SDL2 shared libraries that ship with your Android SDK/NDK builds in
this directory so CMake can find them when configuring with the Android
toolchain.

Expected layout:

```
libs/sdl/android/
  arm64-v8a/
    libSDL2.so
  armeabi-v7a/
    libSDL2.so
  x86/
    libSDL2.so
  x86_64/
    libSDL2.so
```

Only the ABI you are targeting needs to be populated. The headers that match
these libraries already live under `libs/sdl/include`.


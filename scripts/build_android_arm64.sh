#!/bin/bash
set -e
export JAVA_HOME='/c/Program Files/Java/jdk8u422-b05'
export PATH="$JAVA_HOME/bin:$PATH"
export ANDROID_HOME='D:/Android/Sdk'
export ANDROID_NDK_HOME='D:/Android/Sdk/ndk/android-ndk-r14b'

cd 'D:/Project/CPP/renderdoc'
rm -rf build-android-arm64 && mkdir build-android-arm64 && cd build-android-arm64
cmake -G 'MSYS Makefiles' \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DCMAKE_ANDROID_STL_TYPE=c++_static \
  -DBUILD_ANDROID=1 -DANDROID_ABI=arm64-v8a -DANDROID_STL=c++_static \
  -DANDROID_TOOLCHAIN=clang -DCMAKE_BUILD_TYPE=Release -DSTRIP_ANDROID_LIBRARY=On ..
make -j$(nproc)

# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

if (NOT AndroidSDK_FOUND)
  set(ANDROID_SDK "" CACHE PATH "Path to the Android SDK")

  if (ANDROID_SDK)
    set(ANDROID_SDK_SEARCH_DIR "${ANDROID_SDK}")
  elseif(APPLE)
    set(ANDROID_SDK_SEARCH_DIR "$ENV{HOME}/Library/Android/sdk")
  else()
    set(ANDROID_SDK_SEARCH_DIR "/usr/local/lib/android/sdk")
  endif()

  file(GLOB ANDROID_BUILD_TOOLS "${ANDROID_SDK_SEARCH_DIR}/build-tools/*")

  find_program(ANDROID_DX
    NAMES dx
    HINTS ${ANDROID_BUILD_TOOLS}
    DOC "Path to the dx binary")

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(AndroidSDK
    REQUIRED_VARS
      ANDROID_DX
    FAIL_MESSAGE
      "Could NOT find Android SDK. Please provide -DANDROID_SDK=/path/to/android-sdk")
endif()

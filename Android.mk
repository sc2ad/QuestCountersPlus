# Copyright (C) 2009 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


LOCAL_PATH := $(call my-dir)

TARGET_ARCH_ABI := arm64-v8a

rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))

# Build the modloader shared library
include $(CLEAR_VARS)
LOCAL_MODULE	        := modloader
LOCAL_SRC_FILES         := ./extern/libmodloader.so
LOCAL_EXPORT_C_INCLUDES := ./extern/modloader/shared
include $(PREBUILT_SHARED_LIBRARY)

# Build the beatsaber-hook shared library, SPECIFICALLY VERSIONED!
include $(CLEAR_VARS)
LOCAL_MODULE	        := bs-hook
LOCAL_SRC_FILES         := ./extern/debug_libbeatsaber-hook_0_5_2.so
LOCAL_EXPORT_C_INCLUDES := ./extern/beatsaber-hook/shared/
LOCAL_CPP_FEATURES += rtti
LOCAL_EXPORT_C_FLAGS := rtti
include $(PREBUILT_SHARED_LIBRARY)

# Build the customui shared library
# include $(CLEAR_VARS)
# LOCAL_MODULE	        := customui
# LOCAL_SRC_FILES         := ./include/libs/libcustomui_0_1_0.so
# LOCAL_EXPORT_C_INCLUDES := ./extern/customui/shared/
# include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
# Include the two libraries
LOCAL_SHARED_LIBRARIES += modloader
LOCAL_SHARED_LIBRARIES += bs-hook
LOCAL_LDLIBS     := -llog
LOCAL_CFLAGS     := -I'c:/Program Files/Unity/Editor/Data/il2cpp/libil2cpp'
LOCAL_CFLAGS     += -D'MOD_ID="QuestHitscoreVisualizer"' -D'VERSION="4.1.1"' -isystem"./extern" -isystem"./include"
LOCAL_MODULE     := QuestHitscoreVisualizer
LOCAL_CPPFLAGS   := -std=c++2a -Wall -Werror -Wno-unused-function
LOCAL_C_INCLUDES := ./include ./src
LOCAL_SRC_FILES  += $(call rwildcard,src/,*.cpp)
LOCAL_SRC_FILES  += $(call rwildcard,extern/beatsaber-hook/src/inline-hook,*.cpp)
LOCAL_SRC_FILES  += $(call rwildcard,extern/beatsaber-hook/src/inline-hook,*.c)
include $(BUILD_SHARED_LIBRARY)

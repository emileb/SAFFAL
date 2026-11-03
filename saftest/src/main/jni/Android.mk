SAFTEST_PATH := $(call my-dir)

# libsaffal.so from the library's own sources (the :saffal module builds no native code)
include $(SAFTEST_PATH)/../../../../saffal/src/main/jni/Android.mk

LOCAL_PATH := $(SAFTEST_PATH)

include $(CLEAR_VARS)

LOCAL_MODULE := saftest

LOCAL_SRC_FILES := saftest.cpp

LOCAL_CPPFLAGS := -std=c++17 -Wall

LOCAL_SHARED_LIBRARIES := saffal

LOCAL_LDLIBS := -llog -ldl

include $(BUILD_SHARED_LIBRARY)

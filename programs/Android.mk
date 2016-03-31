# Copyright (C) 2016 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := lz4
LOCAL_SRC_FILES := bench.c lz4io.c lz4cli.c
LOCAL_STATIC_LIBRARIES := liblz4
include $(BUILD_HOST_EXECUTABLE)


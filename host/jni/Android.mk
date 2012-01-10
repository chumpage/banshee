LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := host
LOCAL_SRC_FILES := host.cpp ../../common.cpp
LOCAL_CFLAGS := -Wno-multichar \
                -DHAVE_PTHREADS \
                -DANDROID_APP \
                -DEGL_EGLEXT_PROTOTYPES \
                -DGL_GLEXT_PROTOTYPES
LOCAL_C_INCLUDES := $(ANDROID_SRC)/frameworks/base/include
LOCAL_C_INCLUDES += $(ANDROID_SRC)/hardware/libhardware/include
LOCAL_C_INCLUDES += $(ANDROID_SRC)/system/core/include
LOCAL_LDLIBS := $(ANDROID_SRC)/out/target/product/$(ANDROID_PRODUCT)/symbols/system/lib/libui.so
LOCAL_LDLIBS += $(ANDROID_SRC)/out/target/product/$(ANDROID_PRODUCT)/symbols/system/lib/libutils.so
LOCAL_LDLIBS += $(ANDROID_SRC)/out/target/product/$(ANDROID_PRODUCT)/symbols/system/lib/libcutils.so
LOCAL_LDLIBS += -llog -landroid -lEGL -lGLESv2 -lGLESv1_CM
LOCAL_STATIC_LIBRARIES := android_native_app_glue

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/native_app_glue)

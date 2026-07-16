#include "wiretone/core/version.hpp"

#include <jni.h>
#include <string>

extern "C" JNIEXPORT jstring JNICALL
Java_dev_wiretone_receiver_MainActivity_nativeVersion(JNIEnv* environment, jobject) {
    const std::string value = std::string(wiretone::core::product_name()) + " native " +
                              std::string(wiretone::core::version_string());
    return environment->NewStringUTF(value.c_str());
}

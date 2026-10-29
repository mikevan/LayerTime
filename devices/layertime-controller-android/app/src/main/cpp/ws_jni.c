/* Thin JNI layer over ws_core.c (the libwireshark/epan decode path). */
#include <jni.h>
#include <stdint.h>
#include <stdio.h>
#include "ws_core.h"

#define JFN(name) Java_com_projectrevivesolutions_layertime_controller_wireshark_WiresharkDecoder_##name

/* Returns long[5]: {performedNow, ok, epanInitNs, totalInitNs, calls}. */
JNIEXPORT jlongArray JNICALL JFN(nativeInit)(JNIEnv *env, jclass clazz) {
    (void)clazz;
    ltws_init_result r = ltws_init();
    jlong v[5] = { r.performed_now, r.ok, r.epan_init_ns, r.total_init_ns, r.calls };
    jlongArray a = (*env)->NewLongArray(env, 5);
    if (a) (*env)->SetLongArrayRegion(env, a, 0, 5, v);
    return a;
}

JNIEXPORT jstring JNICALL JFN(nativeVersion)(JNIEnv *env, jclass clazz) {
    (void)clazz;
    return (*env)->NewStringUTF(env, ltws_version());
}

static jstring return_and_free(JNIEnv *env, char *s) {
    if (!s) return NULL;
    jstring out = (*env)->NewStringUTF(env, s);
    ltws_free(s);
    return out;
}

/* Returns the field dump, or null if initialization did not succeed. */
JNIEXPORT jstring JNICALL JFN(nativeDecode)(JNIEnv *env, jclass clazz, jbyteArray frame) {
    (void)clazz;
    jsize len = (*env)->GetArrayLength(env, frame);
    jbyte *buf = (*env)->GetByteArrayElements(env, frame, NULL);
    char *s = ltws_decode((const uint8_t *)buf, (int)len);
    (*env)->ReleaseByteArrayElements(env, frame, buf, JNI_ABORT);
    return return_and_free(env, s);
}

/* Memory control: same JNI round trip, no epan dissection. */
JNIEXPORT jstring JNICALL JFN(nativeControl)(JNIEnv *env, jclass clazz, jbyteArray frame) {
    (void)clazz;
    jsize len = (*env)->GetArrayLength(env, frame);
    jbyte *buf = (*env)->GetByteArrayElements(env, frame, NULL);
    char *s = ltws_control((const uint8_t *)buf, (int)len);
    (*env)->ReleaseByteArrayElements(env, frame, buf, JNI_ABORT);
    return return_and_free(env, s);
}

/* Full protocol tree, one record per line: depth\tabbrev\tdisplay\tstart\tlength. */
JNIEXPORT jstring JNICALL JFN(nativeDecodeTree)(JNIEnv *env, jclass clazz, jbyteArray frame) {
    (void)clazz;
    jsize len = (*env)->GetArrayLength(env, frame);
    jbyte *buf = (*env)->GetByteArrayElements(env, frame, NULL);
    char *s = ltws_decode_tree((const uint8_t *)buf, (int)len);
    (*env)->ReleaseByteArrayElements(env, frame, buf, JNI_ABORT);
    return return_and_free(env, s);
}

/* Decode frames from a pcap/pcapng file; returns a tab-separated summary or null. */
JNIEXPORT jstring JNICALL JFN(nativeDecodePcap)(JNIEnv *env, jclass clazz, jstring path, jint maxFrames) {
    (void)clazz;
    const char *p = (*env)->GetStringUTFChars(env, path, NULL);
    const char *err = NULL;
    char *s = ltws_decode_pcap(p, (int)maxFrames, &err);
    (*env)->ReleaseStringUTFChars(env, path, p);
    if (!s) {
        char msg[256];
        snprintf(msg, sizeof msg, "pcap decode failed: %s", err ? err : "unknown");
        return (*env)->NewStringUTF(env, msg);
    }
    return return_and_free(env, s);
}

/* ---- frame-by-frame capture reader (handle = native pointer as long) ---- */

JNIEXPORT jlong JNICALL JFN(nativePcapOpen)(JNIEnv *env, jclass clazz, jstring path) {
    (void)clazz;
    const char *p = (*env)->GetStringUTFChars(env, path, NULL);
    ltws_pcap *h = ltws_pcap_open(p);
    (*env)->ReleaseStringUTFChars(env, path, p);
    return (jlong)(intptr_t)h;
}

JNIEXPORT jstring JNICALL JFN(nativePcapNext)(JNIEnv *env, jclass clazz, jlong handle) {
    (void)clazz;
    if (!handle) return NULL;
    return return_and_free(env, ltws_pcap_next((ltws_pcap *)(intptr_t)handle));
}

JNIEXPORT jstring JNICALL JFN(nativePcapError)(JNIEnv *env, jclass clazz, jlong handle) {
    (void)clazz;
    const char *e = handle ? ltws_pcap_error((ltws_pcap *)(intptr_t)handle) : "No reader.";
    return e ? (*env)->NewStringUTF(env, e) : NULL;
}

JNIEXPORT jstring JNICALL JFN(nativePcapFileType)(JNIEnv *env, jclass clazz, jlong handle) {
    (void)clazz;
    return (*env)->NewStringUTF(env, handle ? ltws_pcap_file_type((ltws_pcap *)(intptr_t)handle) : "unknown");
}

JNIEXPORT jlong JNICALL JFN(nativePcapFileSize)(JNIEnv *env, jclass clazz, jlong handle) {
    (void)env; (void)clazz;
    return handle ? (jlong)ltws_pcap_file_size((ltws_pcap *)(intptr_t)handle) : -1;
}

JNIEXPORT void JNICALL JFN(nativePcapClose)(JNIEnv *env, jclass clazz, jlong handle) {
    (void)env; (void)clazz;
    if (handle) ltws_pcap_close((ltws_pcap *)(intptr_t)handle);
}

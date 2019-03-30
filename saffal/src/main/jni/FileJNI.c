

#include <android/log.h>
#include <jni.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO,"FileJNI NDK", __VA_ARGS__))


static JavaVM* m_jvm;


static JNIEnv *getEnv()
{
    if (!m_jvm)
    {
        LOGI("ERROR, jvm for getEnv is NULL");
        return NULL;
    }

    JNIEnv* jniEnv = 0;

    int status = (*m_jvm)->GetEnv(m_jvm,(void **) &jniEnv, JNI_VERSION_1_4);
    if(status < 0) {

        status = (*m_jvm)->AttachCurrentThread(m_jvm, &jniEnv, NULL);
        if(status < 0) {
            LOGI("getEnv: ERROR failed to attach current thread");
        }
    }

    if (jniEnv)
        LOGI("getEnv env looks OK");
    else
        LOGI("ERROR, getEnv env is NULL");

    return jniEnv;
}

__attribute__((visibility("default"))) jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    LOGI("JNI_OnLoad");
    m_jvm = vm;

    JNIEnv *env = getEnv();

    return JNI_VERSION_1_4;
}



int FileJNI_fopen( const char * filename, const char * mode )
{
    JNIEnv *env = getEnv();

    jstring filenameStr = (*env)->NewStringUTF(env,filename);
    jstring modeStr = (*env)->NewStringUTF(env,mode);

   // Load fopen Java function
   // TODO: These should be cached (PER THREAD). If only one thread access this they can set once and saved
    jclass fopen_cls;
    jmethodID fopen_method;

    fopen_cls = (*env)->FindClass(env,"com/opentouchgaming/saffal/FileJNI");
    fopen_method = (*env)->GetStaticMethodID( env, fopen_cls, "fopen", "(Ljava/lang/String;Ljava/lang/String;)I" );


    // Returns -1 for not in SAF space. 0 for invalid file in SAF. Else a valid FD
    int ret = (*env)->CallStaticIntMethod( env, fopen_cls, fopen_method, filenameStr, modeStr );

    (*env)->DeleteLocalRef( env, filenameStr );
    (*env)->DeleteLocalRef( env, modeStr );

    return ret;
}

int FileJNI_fclose( int fd )
{
    JNIEnv *env = getEnv();

   // Load fopen Java function
    jclass fclose_cls;
    jmethodID fclose_method;

    fclose_cls = (*env)->FindClass(env,"com/opentouchgaming/saffal/FileJNI");
    fclose_method = (*env)->GetStaticMethodID( env, fclose_cls, "fclose", "(I)I" );


    int ret = (*env)->CallStaticIntMethod( env, fclose_cls, fclose_method, fd );

    return ret;
}
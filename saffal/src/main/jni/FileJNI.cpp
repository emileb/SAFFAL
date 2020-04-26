
#include "FileJNI.h"
#include "Utils.h"

#include <android/log.h>
#include <jni.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO,"FileJNI NDK", __VA_ARGS__))

extern "C"
{

	static JavaVM* m_jvm;

	static JNIEnv *getEnv()
	{
		if(!m_jvm)
		{
			LOGI("ERROR, jvm for getEnv is NULL, make sure you load the library first");
			return NULL;
		}

		JNIEnv* jniEnv = 0;

		int status = (m_jvm)->GetEnv((void **) &jniEnv, JNI_VERSION_1_4);

		if(status < 0)
		{

			status = (m_jvm)->AttachCurrentThread(&jniEnv, NULL);

			if(status < 0)
			{
				LOGI("getEnv: ERROR failed to attach current thread");
			}
		}

		if(!jniEnv)
			LOGI("ERROR, getEnv env is NULL");

		return jniEnv;
	}

	__attribute__((visibility("default"))) jint JNI_OnLoad(JavaVM* vm, void* reserved)
	{
		LOGI("JNI_OnLoad");
		m_jvm = vm;
		return JNI_VERSION_1_4;
	}


	void JNICALL Java_com_opentouchgaming_saffal_FileJNI_init(JNIEnv* env, jclass cls, jstring SAFPath)
	{
		const char * SAFPathC = (const char *)(env)->GetStringUTFChars(SAFPath, 0);

		// Save the SAF path so we can check against it in C code
		setSAFPath(SAFPathC);

		env->ReleaseStringUTFChars(SAFPath, SAFPathC);
	}



	int FileJNI_fopen(const char * filename, const char * mode)
	{
		JNIEnv *env = getEnv();

		// Load fopen Java function
		// TODO: These should be cached (PER THREAD). If only one thread access this they can set once and saved
		jclass fopen_cls;
		jmethodID fopen_method;

		fopen_cls = (env)->FindClass("com/opentouchgaming/saffal/FileJNI");
		fopen_method = (env)->GetStaticMethodID(fopen_cls, "fopen", "(Ljava/lang/String;Ljava/lang/String;)I");

		jstring filenameStr = (env)->NewStringUTF(filename);
		jstring modeStr = (env)->NewStringUTF(mode);

		// Call Java function
		// Returns -1 for invalid file in SAF. Else a valid FD
		int ret = (env)->CallStaticIntMethod(fopen_cls, fopen_method, filenameStr, modeStr);

		(env)->DeleteLocalRef(filenameStr);
		(env)->DeleteLocalRef(modeStr);

		return ret;
	}

	int FileJNI_fclose(int fd)
	{
		JNIEnv *env = getEnv();

		// Load fopen Java function
		jclass fclose_cls;
		jmethodID fclose_method;

		fclose_cls = (env)->FindClass("com/opentouchgaming/saffal/FileJNI");
		fclose_method = (env)->GetStaticMethodID(fclose_cls, "fclose", "(I)I");

		int ret = (env)->CallStaticIntMethod(fclose_cls, fclose_method, fd);

		return ret;
	}

	int FileJNI_mkdir(const char * path)
	{
		JNIEnv *env = getEnv();

		// Load fopen Java function
		jclass mkdir_cls;
		jmethodID mkdir_method;

		mkdir_cls = (env)->FindClass("com/opentouchgaming/saffal/FileJNI");
		mkdir_method = (env)->GetStaticMethodID(mkdir_cls, "mkdir", "(Ljava/lang/String;)I");

		jstring pathStr = (env)->NewStringUTF(path);

		// Call Java function
		int ret = (env)->CallStaticIntMethod(mkdir_cls, mkdir_method, pathStr);

		(env)->DeleteLocalRef(pathStr);

		return ret;
	}

	int FileJNI_exists(const char * path)
	{
		JNIEnv *env = getEnv();

		// Load fopen Java function
		jclass exists_cls;
		jmethodID exists_method;

		exists_cls = (env)->FindClass("com/opentouchgaming/saffal/FileJNI");
		exists_method = (env)->GetStaticMethodID(exists_cls, "exists", "(Ljava/lang/String;)I");

		jstring pathStr = (env)->NewStringUTF(path);

		// Call Java function
		int ret = (env)->CallStaticIntMethod(exists_cls, exists_method, pathStr);

		(env)->DeleteLocalRef(pathStr);

		return ret;

	}

}
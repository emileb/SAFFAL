
#include "FileJNI.h"
#include "Utils.h"

#include <android/log.h>
#include <jni.h>
#include <pthread.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO,"FileJNI NDK", __VA_ARGS__))

static pthread_mutex_t lock;


#define MUTEX_LOCK  pthread_mutex_lock(&lock);
#define MUTEX_UNLOCK  if(attached) (m_jvm)->DetachCurrentThread(); pthread_mutex_unlock(&lock);

#define MUTEX_LOCK
#define MUTEX_UNLOCK

extern "C"
{

	static JNIEnv* firstEnv = 0;

	static JavaVM* m_jvm;

	static bool getEnv(JNIEnv **jniEnv)
	{
		bool attached = false;
		int status = 0;

		if(!m_jvm)
		{
			LOGI("ERROR, jvm for getEnv is NULL, make sure you load the library first");
			return false;
		}

		status = (m_jvm)->GetEnv((void **) jniEnv, JNI_VERSION_1_6);

		if(status == JNI_EDETACHED)
		{
			LOGI("getEnv: Thread not attached, attaching...");
			(m_jvm)->DetachCurrentThread();
			status = (m_jvm)->AttachCurrentThread(jniEnv, NULL);

			if(status < 0)
			{
				LOGI("getEnv: ERROR failed to attach current thread");
			}
			attached = true;
		}

		if(!*jniEnv)
			LOGI("ERROR, getEnv env is NULL");

		if(firstEnv == 0)
			firstEnv = *jniEnv;

		return attached;
	}

	static jclass FileJNI_cls;
	static jmethodID fopen_method;
	static jmethodID fclose_method;
	static jmethodID mkdir_method;
	static jmethodID exists_method;

	__attribute__((visibility("default"))) jint JNI_OnLoad(JavaVM* vm, void* reserved)
	{
		LOGI("JNI_OnLoad");
		m_jvm = vm;
		pthread_mutex_init(&lock, NULL);

		JNIEnv *env;
	 	vm->GetEnv ((void **) &env, JNI_VERSION_1_4);

        jclass cls = (env)->FindClass("com/opentouchgaming/saffal/FileJNI");
		FileJNI_cls = (jclass)(env->NewGlobalRef(cls));

		fopen_method = (env)->GetStaticMethodID(FileJNI_cls, "fopen", "(Ljava/lang/String;Ljava/lang/String;)I");
		//fclose_method = (env)->GetStaticMethodID(FileJNI_cls, "fclose", "(I)I");
		mkdir_method = (env)->GetStaticMethodID(FileJNI_cls, "mkdir", "(Ljava/lang/String;)I");
		exists_method = (env)->GetStaticMethodID(FileJNI_cls, "exists", "(Ljava/lang/String;)I");

		return JNI_VERSION_1_6;
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
		MUTEX_LOCK

		JNIEnv *env = NULL;
		bool attached = getEnv(&env);


		jstring filenameStr = (env)->NewStringUTF(filename);
		jstring modeStr = (env)->NewStringUTF(mode);

		// Call Java function
		// Returns -1 for invalid file in SAF. Else a valid FD
		int ret = (env)->CallStaticIntMethod(FileJNI_cls, fopen_method, filenameStr, modeStr);

		(env)->DeleteLocalRef(filenameStr);
		(env)->DeleteLocalRef(modeStr);

		MUTEX_UNLOCK
		return ret;
	}

	int FileJNI_fclose(int fd)
	{
		MUTEX_LOCK

		JNIEnv *env = NULL;
		bool attached = getEnv(&env);

		int ret = (env)->CallStaticIntMethod(FileJNI_cls, fclose_method, fd);

		MUTEX_UNLOCK

		return ret;
	}

	int FileJNI_mkdir(const char * path)
	{
		MUTEX_LOCK

		JNIEnv *env = NULL;
		bool attached = getEnv(&env);

		jstring pathStr = (env)->NewStringUTF(path);

		// Call Java function
		int ret = (env)->CallStaticIntMethod(FileJNI_cls, mkdir_method, pathStr);

		(env)->DeleteLocalRef(pathStr);

		MUTEX_UNLOCK
		return ret;
	}

	int FileJNI_exists(const char * path)
	{
		MUTEX_LOCK

		JNIEnv *env = NULL;
		bool attached = getEnv(&env);

		jstring pathStr = (env)->NewStringUTF(path);

		// Call Java function
		int ret = (env)->CallStaticIntMethod(FileJNI_cls, exists_method, pathStr);

		(env)->DeleteLocalRef(pathStr);

		MUTEX_UNLOCK

		return ret;
	}
}
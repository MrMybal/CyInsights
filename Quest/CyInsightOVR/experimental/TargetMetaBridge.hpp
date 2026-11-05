// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <jni.h>
#include <dlfcn.h>
#include <string>
namespace cylink {
// Communication worker only, under the target UID. The helper comes from the
// authenticated companion's installed, read-only APK; no game file is modified.
class TargetMetaBridge {
    JavaVM* vm=nullptr;jobject helper=nullptr;jmethodID update=nullptr,close=nullptr;
    JNIEnv* Attach(){JNIEnv* env=nullptr;return vm&&vm->AttachCurrentThread(&env,nullptr)==JNI_OK?env:nullptr;}
public:
    explicit TargetMetaBridge(const std::string& apk){
        using GetVMs=jint(*)(JavaVM**,jsize,jsize*);auto get=reinterpret_cast<GetVMs>(dlsym(RTLD_DEFAULT,"JNI_GetCreatedJavaVMs"));
        if(!get){auto library=dlopen("libnativehelper.so",RTLD_NOW|RTLD_LOCAL);if(library)get=reinterpret_cast<GetVMs>(dlsym(library,"JNI_GetCreatedJavaVMs"));}
        jsize count=0;if(!get||get(&vm,1,&count)!=JNI_OK||count!=1){vm=nullptr;return;}
        auto env=Attach();if(!env)return;
        if(env->PushLocalFrame(24)==JNI_OK){auto load=[&]{
            auto activity=env->FindClass("android/app/ActivityThread");if(!activity)return;
            auto current=env->GetStaticMethodID(activity,"currentApplication","()Landroid/app/Application;");if(!current)return;
            auto app=env->CallStaticObjectMethod(activity,current);if(!app||env->ExceptionCheck())return;
            auto context=env->GetObjectClass(app);auto getLoader=env->GetMethodID(context,"getClassLoader","()Ljava/lang/ClassLoader;");if(!getLoader)return;
            auto parent=env->CallObjectMethod(app,getLoader);if(!parent||env->ExceptionCheck())return;
            auto loaderType=env->FindClass("dalvik/system/PathClassLoader");if(!loaderType)return;
            auto construct=env->GetMethodID(loaderType,"<init>","(Ljava/lang/String;Ljava/lang/ClassLoader;)V");if(!construct)return;
            auto path=env->NewStringUTF(apk.c_str());auto loader=env->NewObject(loaderType,construct,path,parent);if(!loader||env->ExceptionCheck())return;
            auto loadClass=env->GetMethodID(loaderType,"loadClass","(Ljava/lang/String;)Ljava/lang/Class;");if(!loadClass)return;
            auto name=env->NewStringUTF("org.cyinsights.ovr.TargetMetaMetrics");auto type=static_cast<jclass>(env->CallObjectMethod(loader,loadClass,name));if(!type||env->ExceptionCheck())return;
            auto constructor=env->GetMethodID(type,"<init>","(Landroid/content/Context;)V");if(!constructor)return;
            auto object=env->NewObject(type,constructor,app);if(!object||env->ExceptionCheck())return;
            update=env->GetMethodID(type,"update","(Ljava/lang/String;)Ljava/lang/String;");close=env->GetMethodID(type,"close","()V");
            if(update&&close&&!env->ExceptionCheck())helper=env->NewGlobalRef(object);
        };load();if(env->ExceptionCheck())env->ExceptionClear();env->PopLocalFrame(nullptr);}else if(env->ExceptionCheck())env->ExceptionClear();
        vm->DetachCurrentThread();
    }
    std::string Update(const std::string& packet){
        if(!helper)return "{\"error\":\"Game HUD helper could not load; completely reopen the selected game\"}";
        auto env=Attach();if(!env)return "{}";std::string out="{}";
        if(env->PushLocalFrame(8)==JNI_OK){auto input=env->NewStringUTF(packet.c_str());auto result=static_cast<jstring>(env->CallObjectMethod(helper,update,input));
            if(result&&!env->ExceptionCheck()){auto text=env->GetStringUTFChars(result,nullptr);if(text){out=text;env->ReleaseStringUTFChars(result,text);}}
            if(env->ExceptionCheck()){env->ExceptionClear();out="{\"error\":\"Game HUD publishing failed\"}";}env->PopLocalFrame(nullptr);
        }else if(env->ExceptionCheck())env->ExceptionClear();vm->DetachCurrentThread();return out;
    }
    ~TargetMetaBridge(){if(!helper)return;auto env=Attach();if(!env)return;env->CallVoidMethod(helper,close);if(env->ExceptionCheck())env->ExceptionClear();env->DeleteGlobalRef(helper);vm->DetachCurrentThread();}
};
}

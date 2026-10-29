// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
// Read-only runtime inquiry: creates no XR instance, session, swapchain or layer.
#include <jni.h>
#define XR_USE_PLATFORM_ANDROID
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <CyGPUInspectorCore/Json.hpp>
#include <dlfcn.h>
#include <mutex>
#include <string>
#include <vector>

extern "C" JNIEXPORT jstring JNICALL
Java_org_cyinsights_ovr_MainActivity_nativeOpenXrCapabilities(JNIEnv* env,jclass,jobject context) {
    using J=cygi::Json;static std::mutex mutex;std::lock_guard<std::mutex> lock(mutex);
    static void* loader=nullptr;static jobject application=nullptr;static std::string cached;
    if(!cached.empty())return env->NewStringUTF(cached.c_str());
    auto out=J::Object();out["source"]="device.system_openxr_loader";out["creates_session"]=false;
    out["status"]="unavailable";out["overlay_supported"]=J();
    do {
        if(!loader)loader=dlopen("libopenxr_loader.so",RTLD_NOW|RTLD_LOCAL);
        if(!loader)loader=dlopen("/system_ext/lib64/libopenxr_loader.so",RTLD_NOW|RTLD_LOCAL);
        if(!loader){const char* why=dlerror();out["reason"]=why?why:"System OpenXR loader could not be loaded";break;}
        auto get=reinterpret_cast<PFN_xrGetInstanceProcAddr>(dlsym(loader,"xrGetInstanceProcAddr"));
        if(!get){out["reason"]="System loader has no xrGetInstanceProcAddr";break;}
        PFN_xrVoidFunction entry=nullptr;auto result=get(XR_NULL_HANDLE,"xrInitializeLoaderKHR",&entry);
        if(XR_FAILED(result)||!entry){out["reason"]="Loader initialization entry unavailable";out["xr_result"]=double(result);break;}
        JavaVM* vm=nullptr;env->GetJavaVM(&vm);if(!application)application=env->NewGlobalRef(context);
        XrLoaderInitInfoAndroidKHR init{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR,nullptr,vm,application};
        result=reinterpret_cast<PFN_xrInitializeLoaderKHR>(entry)(reinterpret_cast<XrLoaderInitInfoBaseHeaderKHR*>(&init));
        if(XR_FAILED(result)){out["reason"]="Android loader initialization failed";out["xr_result"]=double(result);break;}
        entry=nullptr;result=get(XR_NULL_HANDLE,"xrEnumerateInstanceExtensionProperties",&entry);
        if(XR_FAILED(result)||!entry){out["reason"]="Extension enumeration entry unavailable";out["xr_result"]=double(result);break;}
        auto enumerate=reinterpret_cast<PFN_xrEnumerateInstanceExtensionProperties>(entry);uint32_t count=0;
        result=enumerate(nullptr,0,&count,nullptr);
        if(XR_FAILED(result)||count>2048){out["reason"]="Runtime extension discovery failed";out["xr_result"]=double(result);break;}
        std::vector<XrExtensionProperties> properties(count,{XR_TYPE_EXTENSION_PROPERTIES,nullptr,{0},0});
        result=enumerate(nullptr,count,&count,properties.data());
        if(XR_FAILED(result)){out["reason"]="Runtime extension enumeration failed";out["xr_result"]=double(result);break;}
        auto names=J::Array();bool overlay=false;
        for(uint32_t i=0;i<count;++i){std::string name(properties[i].extensionName);names.Push(name);overlay|=name=="XR_EXTX_overlay";}
        out["status"]="measured";out["extension_count"]=double(count);out["extensions"]=std::move(names);out["overlay_supported"]=overlay;
        out["reason"]=overlay?"Runtime advertises XR_EXTX_overlay; an independent session still requires validation":"The device runtime does not advertise XR_EXTX_overlay";
    }while(false);
    cached=out.Write();return env->NewStringUTF(cached.c_str());
}

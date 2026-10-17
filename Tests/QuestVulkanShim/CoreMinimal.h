// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
// Test-only Unreal lifecycle/trace shim. Vulkan calls use the real Quest driver.
#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <functional>
using uint32=uint32_t;using uint64=uint64_t;
#define TEXT(x) L##x
#define VULKANRHI_API
#define VKAPI_TEST
using FString=std::wstring;
using FCriticalSection=std::mutex;
struct FScopeLock{std::unique_lock<std::mutex> lock;explicit FScopeLock(std::mutex* m):lock(*m){}};
struct FMath{template<class T>static T Max(T a,T b){return a>b?a:b;}};
enum class EImmediateFlushType{FlushRHIThread};
struct FRHICommandListImmediate{void ImmediateFlush(EImmediateFlushType){}};
struct MockRenderCommand{template<class F>void operator()(F f){FRHICommandListImmediate commands;f(commands);}};
#define ENQUEUE_RENDER_COMMAND(Name) MockRenderCommand()
inline void FlushRenderingCommands(){}
struct FDelegateHandle{bool valid=false;bool IsValid()const{return valid;}void Reset(){valid=false;}};
struct MockDelegate{void(*callback)()=nullptr;FDelegateHandle AddStatic(void(*f)()){callback=f;return {true};}void Remove(FDelegateHandle){callback=nullptr;}void Broadcast(){if(callback)callback();}};
struct FCoreDelegates{inline static MockDelegate OnEndFrame;};
struct FPlatformTime{
    static uint64 Cycles64(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
    static double Seconds(){return double(Cycles64())*1e-9;}
    static double ToSeconds64(uint64 cycles){return double(cycles)*1e-9;}
};
enum class ERHIInterfaceType{Vulkan};
struct IVulkanDynamicRHI{
    VkInstance instance{};VkDevice device{};VkPhysicalDevice physical{};VkQueue queue{};uint32 family=0;
    ERHIInterfaceType GetInterfaceType()const{return ERHIInterfaceType::Vulkan;}
    VkDevice RHIGetVkDevice(){return device;}VkQueue RHIGetGraphicsVkQueue(){return queue;}
    VkPhysicalDevice RHIGetVkPhysicalDevice(){return physical;}uint32 RHIGetGraphicsQueueFamilyIndex(){return family;}
    void* RHIGetVkInstanceProcAddr(const char* name){return reinterpret_cast<void*>(vkGetInstanceProcAddr(instance,name));}
    void* RHIGetVkDeviceProcAddr(const char* name){return reinterpret_cast<void*>(vkGetDeviceProcAddr(device,name));}
    std::vector<std::string> RHIGetLoadedDeviceExtensions(){return {};}
};
inline IVulkanDynamicRHI MockRHI;
inline IVulkanDynamicRHI* GDynamicRHI=&MockRHI;
inline IVulkanDynamicRHI* GetIVulkanDynamicRHI(){return &MockRHI;}
struct MockTraceField{const char* name;uint64 value;};
struct MockTraceRecord{
    bool open=true;std::vector<MockTraceField> fields;
    inline static std::vector<std::vector<MockTraceField>> records;
    explicit MockTraceRecord(const char*){}
    ~MockTraceRecord(){records.push_back(fields);}
    MockTraceRecord& operator<<(MockTraceField field){fields.push_back(field);return *this;}
#define MOCK_FIELD(Name) template<class T>MockTraceField Name(T value){return {#Name,uint64(value)};}
    MOCK_FIELD(BeginUs) MOCK_FIELD(EndUs) MOCK_FIELD(ObservedCycles) MOCK_FIELD(Submission)
    MOCK_FIELD(Ordinal) MOCK_FIELD(Width) MOCK_FIELD(Height) MOCK_FIELD(Subpasses)
    MOCK_FIELD(PassId) MOCK_FIELD(SubpassIndex) MOCK_FIELD(Flags)
    MOCK_FIELD(DirectDraws) MOCK_FIELD(IndexedDraws) MOCK_FIELD(IndirectCalls) MOCK_FIELD(IndirectCommands)
    MOCK_FIELD(VerticesRequested) MOCK_FIELD(IndicesRequested) MOCK_FIELD(InstancesRequested)
    MOCK_FIELD(GpuUs) MOCK_FIELD(CpuCycles) MOCK_FIELD(MaxErrorNs)
#undef MOCK_FIELD
};
#define UE_TRACE_CHANNEL_DEFINE(Name) inline bool Name=true;
#define UE_TRACE_EVENT_BEGIN(...)
#define UE_TRACE_EVENT_FIELD(...)
#define UE_TRACE_EVENT_END(...)
#define UE_TRACE_CHANNELEXPR_IS_ENABLED(Name) Name
#define UE_TRACE_LOG(Logger,Event,Channel) for(MockTraceRecord Event(#Event);Event.open;Event.open=false) Event

// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
// Opt-in external Android development layer. Sampled tile HUD; requires fresh measured input.
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>
#include <android/log.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <unistd.h>
#include "VulkanHud.hpp"
#include "DrawCounters.hpp"
#include "DrawPublisher.hpp"

namespace {
constexpr const char* Name="VK_LAYER_CYINSIGHTS_heatmap";
std::atomic<unsigned> enumerations{0};
__attribute__((constructor)) void Loaded(){__android_log_print(ANDROID_LOG_INFO,"CyOVRVulkan","LAYER library loaded pid=%d",getpid());}
struct Instance {VkInstance handle;PFN_vkGetInstanceProcAddr gpa;};
struct GeometryPipeline {int topology=-1;bool restart=false,dynamicTopology=false,dynamicRestart=false;};
struct GeometryState {VkPipeline pipeline=VK_NULL_HANDLE;int topology=-1,restart=-1;};
struct Device {VkDevice handle;PFN_vkGetDeviceProcAddr gpa;std::mutex mutex;std::set<VkFramebuffer> seen;unsigned pass_count=0;cytiles::Reader reader;
    cydraw::Book drawBook;std::map<VkPipeline,GeometryPipeline> geometryPipelines;std::map<VkCommandBuffer,GeometryState> geometry;
    std::map<VkRenderPass,cytiles::Pass> passes;std::map<VkCommandBuffer,cytiles::Command> commands;std::map<VkCommandBuffer,VkCommandPool> commandPools;
    VkShaderModule vert=VK_NULL_HANDLE,frag=VK_NULL_HANDLE,label=VK_NULL_HANDLE;VkPipelineLayout layout=VK_NULL_HANDLE;};
std::mutex mutex;
std::map<void*,std::shared_ptr<Instance>> instances;
std::map<VkPhysicalDevice,std::shared_ptr<Instance>> physical;
std::map<void*,std::shared_ptr<Device>> devices;
template<class H> void* Key(H handle){return handle?*reinterpret_cast<void**>(handle):nullptr;}
template<class H> std::shared_ptr<Instance> GetInstance(H handle){std::lock_guard<std::mutex> lock(mutex);auto i=instances.find(Key(handle));return i==instances.end()?nullptr:i->second;}
template<class H> std::shared_ptr<Device> GetDevice(H handle){std::lock_guard<std::mutex> lock(mutex);auto d=devices.find(Key(handle));return d==devices.end()?nullptr:d->second;}
template<class F> F Next(const Device& device,const char* name){return reinterpret_cast<F>(device.gpa(device.handle,name));}
template<class T> T* Chain(const void* start,VkStructureType type){
    auto next=static_cast<const VkBaseInStructure*>(start);for(;next;next=next->pNext)if(next->sType==type){auto info=reinterpret_cast<T*>(const_cast<VkBaseInStructure*>(next));if(info->function==VK_LAYER_LINK_INFO)return info;}return nullptr;
}
void Record(const std::shared_ptr<Device>& device,VkCommandBuffer recording,const VkRenderPassBeginInfo* info,VkSubpassContents contents){
    if(!device||!info)return;std::lock_guard<std::mutex> lock(device->mutex);
    auto& command=device->commands[recording];command.pass=info->renderPass;command.area=info->renderArea;command.subpass=0;command.contents=contents;
    if(device->seen.size()>=32||!device->seen.insert(info->framebuffer).second)return;
    __android_log_print(ANDROID_LOG_INFO,"CyOVRVulkan","PASS rp=%p fb=%p x=%d y=%d width=%u height=%u contents=%d pid=%d",
        info->renderPass,info->framebuffer,info->renderArea.offset.x,info->renderArea.offset.y,info->renderArea.extent.width,info->renderArea.extent.height,int(contents),getpid());
}
}

extern "C" {
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance,const char*);
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice,const char*);
VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceLayerProperties(uint32_t* count,VkLayerProperties* properties){
    if(enumerations.fetch_add(1)==0)__android_log_print(ANDROID_LOG_INFO,"CyOVRVulkan","LAYER properties queried pid=%d",getpid());
    if(!count)return VK_ERROR_INITIALIZATION_FAILED;
    if(!properties){*count=1;return VK_SUCCESS;}uint32_t capacity=*count;*count=std::min(capacity,1u);if(!capacity)return VK_INCOMPLETE;
    *properties={};std::snprintf(properties->layerName,sizeof(properties->layerName),"%s",Name);properties->specVersion=VK_API_VERSION_1_1;properties->implementationVersion=1;
    std::snprintf(properties->description,sizeof(properties->description),"CyInsightOVR development sampled tile HUD");return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceExtensionProperties(const char* layer,uint32_t* count,VkExtensionProperties*){
    if(!count)return VK_ERROR_INITIALIZATION_FAILED;if(layer&&std::strcmp(layer,Name))return VK_ERROR_LAYER_NOT_PRESENT;*count=0;return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(const VkInstanceCreateInfo* info,const VkAllocationCallbacks* alloc,VkInstance* output){
    auto chain=Chain<VkLayerInstanceCreateInfo>(info->pNext,VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO);if(!chain||!chain->u.pLayerInfo){__android_log_print(ANDROID_LOG_ERROR,"CyOVRVulkan","INSTANCE missing loader link pid=%d",getpid());return VK_ERROR_INITIALIZATION_FAILED;}
    auto gpa=chain->u.pLayerInfo->pfnNextGetInstanceProcAddr;chain->u.pLayerInfo=chain->u.pLayerInfo->pNext;
    auto create=reinterpret_cast<PFN_vkCreateInstance>(gpa(VK_NULL_HANDLE,"vkCreateInstance"));if(!create)return VK_ERROR_INITIALIZATION_FAILED;
    VkResult result=create(info,alloc,output);if(result==VK_SUCCESS){auto instance=std::make_shared<Instance>();instance->handle=*output;instance->gpa=gpa;
        {std::lock_guard<std::mutex> lock(mutex);instances[Key(*output)]=instance;}
        __android_log_print(ANDROID_LOG_INFO,"CyOVRVulkan","INSTANCE created api=%u pid=%d",info->pApplicationInfo?info->pApplicationInfo->apiVersion:0,getpid());}
    if(result!=VK_SUCCESS)__android_log_print(ANDROID_LOG_ERROR,"CyOVRVulkan","INSTANCE creation failed result=%d pid=%d",int(result),getpid());return result;
}
VKAPI_ATTR void VKAPI_CALL vkDestroyInstance(VkInstance handle,const VkAllocationCallbacks* alloc){auto instance=GetInstance(handle);if(!instance)return;
    {std::lock_guard<std::mutex> lock(mutex);instances.erase(Key(handle));for(auto i=physical.begin();i!=physical.end();)if(i->second==instance)i=physical.erase(i);else ++i;}
    reinterpret_cast<PFN_vkDestroyInstance>(instance->gpa(handle,"vkDestroyInstance"))(handle,alloc);
}
VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDevices(VkInstance handle,uint32_t* count,VkPhysicalDevice* output){auto instance=GetInstance(handle);if(!instance)return VK_ERROR_INITIALIZATION_FAILED;
    auto result=reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(instance->gpa(handle,"vkEnumeratePhysicalDevices"))(handle,count,output);
    if(output&&(result==VK_SUCCESS||result==VK_INCOMPLETE)){std::lock_guard<std::mutex> lock(mutex);for(uint32_t i=0;i<*count;++i)physical[output[i]]=instance;}return result;
}
VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceLayerProperties(VkPhysicalDevice,uint32_t* count,VkLayerProperties* properties){return vkEnumerateInstanceLayerProperties(count,properties);}
VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceExtensionProperties(VkPhysicalDevice handle,const char* layer,uint32_t* count,VkExtensionProperties* properties){
    if(layer&&std::strcmp(layer,Name)==0){*count=0;return VK_SUCCESS;}std::shared_ptr<Instance> instance;
    {std::lock_guard<std::mutex> lock(mutex);auto i=physical.find(handle);if(i!=physical.end())instance=i->second;}
    if(!instance)instance=GetInstance(handle);if(!instance)return VK_ERROR_INITIALIZATION_FAILED;
    return reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(instance->gpa(instance->handle,"vkEnumerateDeviceExtensionProperties"))(handle,layer,count,properties);
}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice handle,const VkDeviceCreateInfo* info,const VkAllocationCallbacks* alloc,VkDevice* output){
    std::shared_ptr<Instance> instance;{std::lock_guard<std::mutex> lock(mutex);auto i=physical.find(handle);if(i!=physical.end())instance=i->second;}if(!instance)instance=GetInstance(handle);if(!instance)return VK_ERROR_INITIALIZATION_FAILED;
    auto chain=Chain<VkLayerDeviceCreateInfo>(info->pNext,VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO);if(!chain||!chain->u.pLayerInfo)return VK_ERROR_INITIALIZATION_FAILED;
    auto gpa=chain->u.pLayerInfo->pfnNextGetInstanceProcAddr;auto gdpa=chain->u.pLayerInfo->pfnNextGetDeviceProcAddr;chain->u.pLayerInfo=chain->u.pLayerInfo->pNext;
    auto result=reinterpret_cast<PFN_vkCreateDevice>(gpa(instance->handle,"vkCreateDevice"))(handle,info,alloc,output);
    if(result==VK_SUCCESS){auto device=std::make_shared<Device>();device->handle=*output;device->gpa=gdpa;{std::lock_guard<std::mutex> lock(mutex);devices[Key(*output)]=device;}
        __android_log_print(ANDROID_LOG_INFO,"CyOVRVulkan","DEVICE created pid=%d; experimental tile HUD; waiting for a fresh measured feed",getpid());}return result;
}
VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(VkDevice handle,const VkAllocationCallbacks* alloc){auto device=GetDevice(handle);if(!device)return;
    cydraw::Output().Forget(reinterpret_cast<uintptr_t>(device.get()));
    {std::lock_guard<std::mutex> lock(device->mutex);cytiles::Cleanup(*device);}
    {std::lock_guard<std::mutex> lock(mutex);devices.erase(Key(handle));}Next<PFN_vkDestroyDevice>(*device,"vkDestroyDevice")(handle,alloc);
}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateRenderPass(VkDevice handle,const VkRenderPassCreateInfo* info,const VkAllocationCallbacks* alloc,VkRenderPass* output){
    auto device=GetDevice(handle);if(!device)return VK_ERROR_INITIALIZATION_FAILED;auto result=Next<PFN_vkCreateRenderPass>(*device,"vkCreateRenderPass")(handle,info,alloc,output);
    if(result==VK_SUCCESS){std::lock_guard<std::mutex> lock(device->mutex);auto& pass=device->passes[*output];pass.samples.resize(info->subpassCount,VK_SAMPLE_COUNT_1_BIT);pass.color.resize(info->subpassCount,false);pass.pipelines.resize(info->subpassCount,VK_NULL_HANDLE);pass.labels.resize(info->subpassCount,VK_NULL_HANDLE);
        for(uint32_t i=0;i<info->subpassCount;++i){const auto& sub=info->pSubpasses[i];if(sub.colorAttachmentCount==1&&sub.pColorAttachments&&sub.pColorAttachments[0].attachment<info->attachmentCount){pass.color[i]=true;pass.samples[i]=info->pAttachments[sub.pColorAttachments[0].attachment].samples;}}
        if(device->pass_count++<32){uint32_t views=0;for(auto p=static_cast<const VkBaseInStructure*>(info->pNext);p;p=p->pNext)if(p->sType==VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO){auto m=reinterpret_cast<const VkRenderPassMultiviewCreateInfo*>(p);for(uint32_t i=0;i<m->subpassCount;++i)views|=m->pViewMasks[i];}
        __android_log_print(ANDROID_LOG_INFO,"CyOVRVulkan","RENDERPASS rp=%p attachments=%u subpasses=%u view_mask=%u",*output,info->attachmentCount,info->subpassCount,views);}}return result;
}
VKAPI_ATTR void VKAPI_CALL vkCmdBeginRenderPass(VkCommandBuffer command,const VkRenderPassBeginInfo* info,VkSubpassContents contents){auto device=GetDevice(command);if(!device)return;Record(device,command,info,contents);Next<PFN_vkCmdBeginRenderPass>(*device,"vkCmdBeginRenderPass")(command,info,contents);}
VKAPI_ATTR void VKAPI_CALL vkCmdBeginRenderPass2(VkCommandBuffer command,const VkRenderPassBeginInfo* info,const VkSubpassBeginInfo* subpass){auto device=GetDevice(command);if(!device)return;Record(device,command,info,subpass->contents);Next<PFN_vkCmdBeginRenderPass2>(*device,"vkCmdBeginRenderPass2")(command,info,subpass);}
VKAPI_ATTR void VKAPI_CALL vkCmdBeginRenderPass2KHR(VkCommandBuffer command,const VkRenderPassBeginInfo* info,const VkSubpassBeginInfo* subpass){auto device=GetDevice(command);if(!device)return;Record(device,command,info,subpass->contents);Next<PFN_vkCmdBeginRenderPass2KHR>(*device,"vkCmdBeginRenderPass2KHR")(command,info,subpass);}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateRenderPass2(VkDevice handle,const VkRenderPassCreateInfo2* info,const VkAllocationCallbacks* alloc,VkRenderPass* output){
    auto device=GetDevice(handle);if(!device)return VK_ERROR_INITIALIZATION_FAILED;auto result=Next<PFN_vkCreateRenderPass2>(*device,"vkCreateRenderPass2")(handle,info,alloc,output);
    if(result==VK_SUCCESS){std::lock_guard<std::mutex> lock(device->mutex);auto& pass=device->passes[*output];pass.samples.resize(info->subpassCount,VK_SAMPLE_COUNT_1_BIT);pass.color.resize(info->subpassCount,false);pass.pipelines.resize(info->subpassCount,VK_NULL_HANDLE);pass.labels.resize(info->subpassCount,VK_NULL_HANDLE);
        for(uint32_t i=0;i<info->subpassCount;++i){const auto& sub=info->pSubpasses[i];if(sub.colorAttachmentCount==1&&sub.pColorAttachments&&sub.pColorAttachments[0].attachment<info->attachmentCount){pass.color[i]=true;pass.samples[i]=info->pAttachments[sub.pColorAttachments[0].attachment].samples;}}
    }return result;
}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateRenderPass2KHR(VkDevice handle,const VkRenderPassCreateInfo2* info,const VkAllocationCallbacks* alloc,VkRenderPass* output){
    auto device=GetDevice(handle);if(!device)return VK_ERROR_INITIALIZATION_FAILED;auto result=Next<PFN_vkCreateRenderPass2KHR>(*device,"vkCreateRenderPass2KHR")(handle,info,alloc,output);
    if(result==VK_SUCCESS){std::lock_guard<std::mutex> lock(device->mutex);auto& pass=device->passes[*output];pass.samples.resize(info->subpassCount,VK_SAMPLE_COUNT_1_BIT);pass.color.resize(info->subpassCount,false);pass.pipelines.resize(info->subpassCount,VK_NULL_HANDLE);pass.labels.resize(info->subpassCount,VK_NULL_HANDLE);
        for(uint32_t i=0;i<info->subpassCount;++i){const auto& sub=info->pSubpasses[i];if(sub.colorAttachmentCount==1&&sub.pColorAttachments&&sub.pColorAttachments[0].attachment<info->attachmentCount){pass.color[i]=true;pass.samples[i]=info->pAttachments[sub.pColorAttachments[0].attachment].samples;}}
    }return result;
}
VKAPI_ATTR void VKAPI_CALL vkDestroyRenderPass(VkDevice handle,VkRenderPass pass,const VkAllocationCallbacks* alloc){auto device=GetDevice(handle);if(!device)return;{std::lock_guard<std::mutex> lock(device->mutex);auto it=device->passes.find(pass);if(it!=device->passes.end()){cytiles::DestroyPass(*device,it->second);device->passes.erase(it);}}Next<PFN_vkDestroyRenderPass>(*device,"vkDestroyRenderPass")(handle,pass,alloc);}
VKAPI_ATTR VkResult VKAPI_CALL vkAllocateCommandBuffers(VkDevice handle,const VkCommandBufferAllocateInfo* info,VkCommandBuffer* output){auto device=GetDevice(handle);if(!device)return VK_ERROR_INITIALIZATION_FAILED;auto result=Next<PFN_vkAllocateCommandBuffers>(*device,"vkAllocateCommandBuffers")(handle,info,output);if(result==VK_SUCCESS){std::lock_guard<std::mutex> lock(device->mutex);for(uint32_t i=0;i<info->commandBufferCount;++i)device->commandPools[output[i]]=info->commandPool;}return result;}
VKAPI_ATTR void VKAPI_CALL vkDestroyCommandPool(VkDevice handle,VkCommandPool pool,const VkAllocationCallbacks* alloc){auto device=GetDevice(handle);if(!device)return;{std::lock_guard<std::mutex> lock(device->mutex);for(auto i=device->commandPools.begin();i!=device->commandPools.end();)if(i->second==pool){device->commands.erase(i->first);device->drawBook.Erase(reinterpret_cast<uintptr_t>(i->first));device->geometry.erase(i->first);i=device->commandPools.erase(i);}else ++i;}Next<PFN_vkDestroyCommandPool>(*device,"vkDestroyCommandPool")(handle,pool,alloc);}
VKAPI_ATTR VkResult VKAPI_CALL vkResetCommandPool(VkDevice handle,VkCommandPool pool,VkCommandPoolResetFlags flags){auto device=GetDevice(handle);if(!device)return VK_ERROR_INITIALIZATION_FAILED;auto result=Next<PFN_vkResetCommandPool>(*device,"vkResetCommandPool")(handle,pool,flags);if(result==VK_SUCCESS){std::lock_guard<std::mutex> lock(device->mutex);for(const auto& c:device->commandPools)if(c.second==pool){device->commands.erase(c.first);device->drawBook.Erase(reinterpret_cast<uintptr_t>(c.first));device->geometry.erase(c.first);}}return result;}
VKAPI_ATTR VkResult VKAPI_CALL vkBeginCommandBuffer(VkCommandBuffer command,const VkCommandBufferBeginInfo* info){auto device=GetDevice(command);if(!device)return VK_ERROR_INITIALIZATION_FAILED;auto result=Next<PFN_vkBeginCommandBuffer>(*device,"vkBeginCommandBuffer")(command,info);if(result==VK_SUCCESS){std::lock_guard<std::mutex> lock(device->mutex);device->commands[command]={};device->drawBook.Begin(reinterpret_cast<uintptr_t>(command));device->geometry[command]={};}return result;}
VKAPI_ATTR void VKAPI_CALL vkFreeCommandBuffers(VkDevice handle,VkCommandPool pool,uint32_t count,const VkCommandBuffer* buffers){auto device=GetDevice(handle);if(!device)return;{std::lock_guard<std::mutex> lock(device->mutex);for(uint32_t i=0;i<count;++i){device->commands.erase(buffers[i]);device->commandPools.erase(buffers[i]);device->drawBook.Erase(reinterpret_cast<uintptr_t>(buffers[i]));device->geometry.erase(buffers[i]);}}Next<PFN_vkFreeCommandBuffers>(*device,"vkFreeCommandBuffers")(handle,pool,count,buffers);}
VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer command,VkPipelineBindPoint point,VkPipeline pipeline){auto device=GetDevice(command);if(!device)return;if(point==VK_PIPELINE_BIND_POINT_GRAPHICS){std::lock_guard<std::mutex> lock(device->mutex);device->commands[command].pipeline=pipeline;device->geometry[command].pipeline=pipeline;}Next<PFN_vkCmdBindPipeline>(*device,"vkCmdBindPipeline")(command,point,pipeline);}
VKAPI_ATTR void VKAPI_CALL vkCmdSetViewport(VkCommandBuffer command,uint32_t first,uint32_t count,const VkViewport* values){auto device=GetDevice(command);if(!device)return;if(first==0&&count){std::lock_guard<std::mutex> lock(device->mutex);auto& c=device->commands[command];c.viewport=values[0];c.hasViewport=true;}Next<PFN_vkCmdSetViewport>(*device,"vkCmdSetViewport")(command,first,count,values);}
VKAPI_ATTR void VKAPI_CALL vkCmdSetScissor(VkCommandBuffer command,uint32_t first,uint32_t count,const VkRect2D* values){auto device=GetDevice(command);if(!device)return;if(first==0&&count){std::lock_guard<std::mutex> lock(device->mutex);auto& c=device->commands[command];c.scissor=values[0];c.hasScissor=true;}Next<PFN_vkCmdSetScissor>(*device,"vkCmdSetScissor")(command,first,count,values);}
VKAPI_ATTR void VKAPI_CALL vkCmdSetBlendConstants(VkCommandBuffer command,const float values[4]){auto device=GetDevice(command);if(!device)return;{std::lock_guard<std::mutex> lock(device->mutex);std::copy(values,values+4,device->commands[command].blend.begin());}Next<PFN_vkCmdSetBlendConstants>(*device,"vkCmdSetBlendConstants")(command,values);}
VKAPI_ATTR void VKAPI_CALL vkCmdNextSubpass(VkCommandBuffer command,VkSubpassContents contents){auto device=GetDevice(command);if(!device)return;Next<PFN_vkCmdNextSubpass>(*device,"vkCmdNextSubpass")(command,contents);{std::lock_guard<std::mutex> lock(device->mutex);auto& c=device->commands[command];++c.subpass;c.contents=contents;}}
VKAPI_ATTR void VKAPI_CALL vkCmdNextSubpass2(VkCommandBuffer command,const VkSubpassBeginInfo* begin,const VkSubpassEndInfo* end){auto device=GetDevice(command);if(!device)return;Next<PFN_vkCmdNextSubpass2>(*device,"vkCmdNextSubpass2")(command,begin,end);{std::lock_guard<std::mutex> lock(device->mutex);auto& c=device->commands[command];++c.subpass;c.contents=begin->contents;}}
VKAPI_ATTR void VKAPI_CALL vkCmdNextSubpass2KHR(VkCommandBuffer command,const VkSubpassBeginInfo* begin,const VkSubpassEndInfo* end){auto device=GetDevice(command);if(!device)return;Next<PFN_vkCmdNextSubpass2KHR>(*device,"vkCmdNextSubpass2KHR")(command,begin,end);{std::lock_guard<std::mutex> lock(device->mutex);auto& c=device->commands[command];++c.subpass;c.contents=begin->contents;}}
VKAPI_ATTR void VKAPI_CALL vkCmdEndRenderPass(VkCommandBuffer command){auto device=GetDevice(command);if(!device)return;cytiles::Draw(*device,command);Next<PFN_vkCmdEndRenderPass>(*device,"vkCmdEndRenderPass")(command);}
VKAPI_ATTR void VKAPI_CALL vkCmdEndRenderPass2(VkCommandBuffer command,const VkSubpassEndInfo* end){auto device=GetDevice(command);if(!device)return;cytiles::Draw(*device,command);Next<PFN_vkCmdEndRenderPass2>(*device,"vkCmdEndRenderPass2")(command,end);}
VKAPI_ATTR void VKAPI_CALL vkCmdEndRenderPass2KHR(VkCommandBuffer command,const VkSubpassEndInfo* end){auto device=GetDevice(command);if(!device)return;cytiles::Draw(*device,command);Next<PFN_vkCmdEndRenderPass2KHR>(*device,"vkCmdEndRenderPass2KHR")(command,end);}

}
#include "DrawHooks.hpp"
namespace {
PFN_vkVoidFunction Hook(const char* name){
#define MATCH(f) if(std::strcmp(name,#f)==0)return reinterpret_cast<PFN_vkVoidFunction>(f)
    DRAW_HOOKS(MATCH);
    MATCH(vkGetInstanceProcAddr);MATCH(vkGetDeviceProcAddr);MATCH(vkCreateInstance);MATCH(vkDestroyInstance);MATCH(vkEnumeratePhysicalDevices);
    MATCH(vkEnumerateInstanceLayerProperties);MATCH(vkEnumerateInstanceExtensionProperties);MATCH(vkEnumerateDeviceLayerProperties);MATCH(vkEnumerateDeviceExtensionProperties);
    MATCH(vkCreateRenderPass2);MATCH(vkCreateRenderPass2KHR);MATCH(vkDestroyRenderPass);MATCH(vkBeginCommandBuffer);MATCH(vkFreeCommandBuffers);MATCH(vkAllocateCommandBuffers);MATCH(vkDestroyCommandPool);MATCH(vkResetCommandPool);
    MATCH(vkCmdBindPipeline);MATCH(vkCmdSetViewport);MATCH(vkCmdSetScissor);MATCH(vkCmdSetBlendConstants);MATCH(vkCmdNextSubpass);MATCH(vkCmdNextSubpass2);MATCH(vkCmdNextSubpass2KHR);MATCH(vkCmdEndRenderPass);MATCH(vkCmdEndRenderPass2);MATCH(vkCmdEndRenderPass2KHR);
    MATCH(vkCreateDevice);MATCH(vkDestroyDevice);MATCH(vkCreateRenderPass);MATCH(vkCmdBeginRenderPass);MATCH(vkCmdBeginRenderPass2);MATCH(vkCmdBeginRenderPass2KHR);
#undef MATCH
    return nullptr;
}
}
extern "C" {
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance handle,const char* name){if(!name)return nullptr;auto hook=Hook(name);
    if(!handle)return hook;auto instance=GetInstance(handle);if(!instance)return nullptr;auto next=instance->gpa(handle,name);return next&&hook?hook:next;
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice handle,const char* name){if(!name)return nullptr;auto device=GetDevice(handle);if(!device)return nullptr;auto next=device->gpa(handle,name);auto hook=Hook(name);return next&&hook?hook:next;}
VKAPI_ATTR VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface* version){if(!version||version->loaderLayerInterfaceVersion<1)return VK_ERROR_INITIALIZATION_FAILED;
    version->loaderLayerInterfaceVersion=std::min(2u,version->loaderLayerInterfaceVersion);version->pfnGetInstanceProcAddr=vkGetInstanceProcAddr;version->pfnGetDeviceProcAddr=vkGetDeviceProcAddr;version->pfnGetPhysicalDeviceProcAddr=vkGetInstanceProcAddr;return VK_SUCCESS;
}
}

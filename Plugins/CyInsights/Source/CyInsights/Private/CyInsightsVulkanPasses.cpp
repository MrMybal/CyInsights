// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#include "CyInsightsVulkanPasses.h"
#if CYINSIGHTS_VULKAN_PASSES
#include "IVulkanDynamicRHI.h"
#include "GPUProfiler.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "Misc/ScopeLock.h"
#include "Misc/CoreDelegates.h"
#include "HAL/PlatformTime.h"
#include "CyInsightsTrace.h"
#include "QuestPassClock.h"
#include "QuestPassContents.h"
#include "Trace/Trace.inl"
#include <atomic>
#include <map>
#include <vector>

namespace VulkanDynamicAPI {
#define CY_VK_DECLARE(Name) extern VULKANRHI_API PFN_##Name Name;
CY_VK_DECLARE(vkBeginCommandBuffer) CY_VK_DECLARE(vkFreeCommandBuffers)
CY_VK_DECLARE(vkAllocateCommandBuffers) CY_VK_DECLARE(vkDestroyCommandPool)
CY_VK_DECLARE(vkCmdBeginRenderPass) CY_VK_DECLARE(vkCmdBeginRenderPass2KHR)
CY_VK_DECLARE(vkCmdEndRenderPass) CY_VK_DECLARE(vkCmdEndRenderPass2KHR) CY_VK_DECLARE(vkCmdNextSubpass)
CY_VK_DECLARE(vkQueueSubmit) CY_VK_DECLARE(vkQueueSubmit2KHR)
CY_VK_DECLARE(vkResetFences) CY_VK_DECLARE(vkDestroyFence)
CY_VK_DECLARE(vkCmdDraw) CY_VK_DECLARE(vkCmdDrawIndexed)
CY_VK_DECLARE(vkCmdDrawIndirect) CY_VK_DECLARE(vkCmdDrawIndexedIndirect) CY_VK_DECLARE(vkCmdExecuteCommands)
#undef CY_VK_DECLARE
}
UE_TRACE_CHANNEL_DEFINE(CyInsightsVulkanChannel);
UE_TRACE_EVENT_BEGIN(CyInsightsVulkan,Pass)
    UE_TRACE_EVENT_FIELD(uint64,BeginUs)
    UE_TRACE_EVENT_FIELD(uint64,EndUs)
    UE_TRACE_EVENT_FIELD(uint64,ObservedCycles)
    UE_TRACE_EVENT_FIELD(uint64,Submission)
    UE_TRACE_EVENT_FIELD(uint32,Ordinal)
    UE_TRACE_EVENT_FIELD(uint32,Width)
    UE_TRACE_EVENT_FIELD(uint32,Height)
    UE_TRACE_EVENT_FIELD(uint32,Subpasses)
UE_TRACE_EVENT_END()
UE_TRACE_EVENT_BEGIN(CyInsightsVulkan,Contents)
    UE_TRACE_EVENT_FIELD(uint64,BeginUs)
    UE_TRACE_EVENT_FIELD(uint64,EndUs)
    UE_TRACE_EVENT_FIELD(uint64,PassId)
    UE_TRACE_EVENT_FIELD(uint64,Submission)
    UE_TRACE_EVENT_FIELD(uint32,Ordinal)
    UE_TRACE_EVENT_FIELD(uint32,Width)
    UE_TRACE_EVENT_FIELD(uint32,Height)
    UE_TRACE_EVENT_FIELD(uint32,SubpassIndex)
    UE_TRACE_EVENT_FIELD(uint32,Subpasses)
    UE_TRACE_EVENT_FIELD(uint32,Flags)
    UE_TRACE_EVENT_FIELD(uint64,DirectDraws)
    UE_TRACE_EVENT_FIELD(uint64,IndexedDraws)
    UE_TRACE_EVENT_FIELD(uint64,IndirectCalls)
    UE_TRACE_EVENT_FIELD(uint64,IndirectCommands)
    UE_TRACE_EVENT_FIELD(uint64,VerticesRequested)
    UE_TRACE_EVENT_FIELD(uint64,IndicesRequested)
    UE_TRACE_EVENT_FIELD(uint64,InstancesRequested)
UE_TRACE_EVENT_END()
UE_TRACE_EVENT_BEGIN(CyInsightsVulkan,Clock)
    UE_TRACE_EVENT_FIELD(uint64,GpuUs)
    UE_TRACE_EVENT_FIELD(uint64,CpuCycles)
    UE_TRACE_EVENT_FIELD(double,MaxErrorNs)
UE_TRACE_EVENT_END()
namespace {
// GPU query pools belong to command buffers, never to game frames. Unreal owns
// submission fences and buffer reuse. No wait, extra submission, render-pass
// split, image copy or buffer copy is introduced during capture.
constexpr uint32 QueriesPerPool=256;
struct Pass {uint32 block,query,ordinal,width,height,subpasses=1;bool ended=false;uint64 epoch,id;std::vector<cyinsights_quest::PassWork> work;};
struct Buffer {
    std::vector<VkQueryPool> pools;std::vector<Pass> passes;
    VkFence fence=VK_NULL_HANDLE;uint64 submission=0;
    bool primary=false,submitted=false,inside=false;
    uint32 ordinal=0;uint64 record_epoch=0;cyinsights_quest::PassWork secondary;
    VkCommandPool owner=VK_NULL_HANDLE;
};
FCriticalSection Mutex;
std::map<VkCommandBuffer,Buffer> Buffers;
std::atomic<bool> Enabled{false};
uint64 Epoch=0,Submission=0,ResolvedCount=0,DroppedCount=0,PassSerial=0;
VkDevice Device=VK_NULL_HANDLE;VkQueue Graphics=VK_NULL_HANDLE;
double Frequency=0;FString Reason=TEXT("Not started");
uint32 TimestampBits=0;
FDelegateHandle PollHandle;
PFN_vkCreateQueryPool CreatePool=nullptr;PFN_vkDestroyQueryPool DestroyPool=nullptr;
PFN_vkCmdResetQueryPool ResetPool=nullptr;PFN_vkCmdWriteTimestamp Timestamp=nullptr;
PFN_vkGetQueryPoolResults Results=nullptr;PFN_vkGetFenceStatus FenceStatus=nullptr;
PFN_vkDeviceWaitIdle DeviceIdle=nullptr;
PFN_vkGetCalibratedTimestampsEXT Calibrated=nullptr;
double LastClock=0;
#define CY_VK_NEXT(Name) PFN_##Name Next_##Name=nullptr;
CY_VK_NEXT(vkBeginCommandBuffer) CY_VK_NEXT(vkFreeCommandBuffers)
CY_VK_NEXT(vkAllocateCommandBuffers) CY_VK_NEXT(vkDestroyCommandPool)
CY_VK_NEXT(vkCmdBeginRenderPass) CY_VK_NEXT(vkCmdBeginRenderPass2KHR)
CY_VK_NEXT(vkCmdEndRenderPass) CY_VK_NEXT(vkCmdEndRenderPass2KHR) CY_VK_NEXT(vkCmdNextSubpass)
CY_VK_NEXT(vkQueueSubmit) CY_VK_NEXT(vkQueueSubmit2KHR)
CY_VK_NEXT(vkResetFences) CY_VK_NEXT(vkDestroyFence)
CY_VK_NEXT(vkCmdDraw) CY_VK_NEXT(vkCmdDrawIndexed)
CY_VK_NEXT(vkCmdDrawIndirect) CY_VK_NEXT(vkCmdDrawIndexedIndirect) CY_VK_NEXT(vkCmdExecuteCommands)
#undef CY_VK_NEXT
void Resolve(Buffer& B,bool KnownComplete=false) {
    if(!B.submitted||B.passes.empty())return;
    // Availability alone can still contain the preceding generation's values
    // until the GPU executes vkCmdResetQueryPool. Observe the engine fence, or
    // the Vulkan-guaranteed completion at command-buffer reuse/free, first.
    if(!KnownComplete&&(!B.fence||FenceStatus(Device,B.fence)!=VK_SUCCESS))return;
    struct Reading {uint64 value,available;};
    std::vector<Reading> Values(QueriesPerPool);
    for(uint32 Block=0;Block<B.pools.size();++Block) {
        uint32 Count=0;
        for(const auto& P:B.passes)if(P.block==Block&&P.ended)Count=FMath::Max(Count,P.query+2);
        if(!Count)continue;
        const auto Result=Results(Device,B.pools[Block],0,Count,Count*sizeof(Reading),Values.data(),sizeof(Reading),VK_QUERY_RESULT_64_BIT|VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
        for(const auto& P:B.passes)if(P.block==Block) {
            if(P.epoch!=Epoch)continue;
            uint64_t Span=0;
            if(!P.ended||Result!=VK_SUCCESS||!Values[P.query].available||!Values[P.query+1].available||!cyinsights_quest::TimestampSpan(Values[P.query].value,Values[P.query+1].value,TimestampBits,Span)){++DroppedCount;continue;}
            const uint64 Begin=uint64(double(Values[P.query].value)/Frequency*1e6);
            const uint64 End=Begin+uint64(double(Span)/Frequency*1e6);
            if(Enabled.load(std::memory_order_relaxed)&&UE_TRACE_CHANNELEXPR_IS_ENABLED(CyInsightsVulkanChannel)) {
                UE_TRACE_LOG(CyInsightsVulkan,Pass,CyInsightsVulkanChannel)
                    << Pass.BeginUs(Begin) << Pass.EndUs(End)
                    << Pass.ObservedCycles(FPlatformTime::Cycles64())
                    << Pass.Submission(B.submission) << Pass.Ordinal(P.ordinal)
                    << Pass.Width(P.width) << Pass.Height(P.height) << Pass.Subpasses(P.subpasses);
                for(uint32 Index=0;Index<P.work.size();++Index){const auto& Work=P.work[Index];
                    UE_TRACE_LOG(CyInsightsVulkan,Contents,CyInsightsVulkanChannel)
                        << Contents.BeginUs(Begin) << Contents.EndUs(End) << Contents.PassId(P.id)
                        << Contents.Submission(B.submission) << Contents.Ordinal(P.ordinal)
                        << Contents.Width(P.width) << Contents.Height(P.height)
                        << Contents.SubpassIndex(Index) << Contents.Subpasses(P.subpasses) << Contents.Flags(Work.flags)
                        << Contents.DirectDraws(Work.direct) << Contents.IndexedDraws(Work.indexed)
                        << Contents.IndirectCalls(Work.indirect) << Contents.IndirectCommands(Work.indirect_commands)
                        << Contents.VerticesRequested(Work.vertices) << Contents.IndicesRequested(Work.indices)
                        << Contents.InstancesRequested(Work.instances);
                }
                ++ResolvedCount;
            }
        }
    }
    B.passes.clear();B.submitted=false;B.fence=VK_NULL_HANDLE;
}
void PollLocked(){
    if(!Device||!Results||!FenceStatus)return;
    if(Enabled.load()&&Calibrated&&FPlatformTime::Seconds()-LastClock>=1.) {
        VkCalibratedTimestampInfoEXT Info{};Info.sType=VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT;Info.timeDomain=VK_TIME_DOMAIN_DEVICE_EXT;
        uint64_t Tick=0,Deviation=0;const auto Begin=FPlatformTime::Cycles64();
        const auto Result=Calibrated(Device,1,&Info,&Tick,&Deviation);const auto End=FPlatformTime::Cycles64();
        LastClock=FPlatformTime::Seconds();
        if(Result==VK_SUCCESS){UE_TRACE_LOG(CyInsightsVulkan,Clock,CyInsightsVulkanChannel)
            << Clock.GpuUs(uint64(double(Tick)/Frequency*1e6)) << Clock.CpuCycles(Begin+(End-Begin)/2)
            << Clock.MaxErrorNs(double(Deviation)+FPlatformTime::ToSeconds64(End-Begin)*5e8);}
    }
    for(auto& Entry:Buffers)Resolve(Entry.second);
}
void Poll(){FScopeLock Lock(&Mutex);PollLocked();}
void BeginPass(VkCommandBuffer Cmd,const VkRenderPassBeginInfo* Info) {
    if(!Enabled.load(std::memory_order_relaxed)||!Info)return;
    FScopeLock Lock(&Mutex);if(!Enabled.load(std::memory_order_relaxed))return;auto Found=Buffers.find(Cmd);
    if(Found==Buffers.end()||!Found->second.primary||Found->second.inside)return;
    auto& B=Found->second;const uint32 Ordinal=++B.ordinal;
    const uint32 Index=uint32(B.passes.size()),Block=Index/(QueriesPerPool/2),Query=(Index%(QueriesPerPool/2))*2;
    if(Block==B.pools.size()) {
        VkQueryPoolCreateInfo Create{};Create.sType=VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;Create.queryType=VK_QUERY_TYPE_TIMESTAMP;Create.queryCount=QueriesPerPool;
        VkQueryPool Pool=VK_NULL_HANDLE;
        if(CreatePool(Device,&Create,nullptr,&Pool)!=VK_SUCCESS){++DroppedCount;return;}
        B.pools.push_back(Pool);
    }
    // This is immediately BEFORE the application's render-pass begin command.
    if(!Query)ResetPool(Cmd,B.pools[Block],0,QueriesPerPool);
    Timestamp(Cmd,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,B.pools[Block],Query);
    B.passes.push_back({Block,Query,Ordinal,Info->renderArea.extent.width,Info->renderArea.extent.height,1,false,Epoch,++PassSerial,{cyinsights_quest::PassWork{}}});B.inside=true;
}
void EndPass(VkCommandBuffer Cmd) {
    FScopeLock Lock(&Mutex);auto Found=Buffers.find(Cmd);if(Found==Buffers.end())return;
    auto& B=Found->second;if(!B.inside||B.passes.empty())return;
    auto& P=B.passes.back();Timestamp(Cmd,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,B.pools[P.block],P.query+1);P.ended=true;B.inside=false;
}
VKAPI_ATTR VkResult VKAPI_CALL BeginBuffer(VkCommandBuffer Cmd,const VkCommandBufferBeginInfo* Info) {
    // vkBeginCommandBuffer requires a non-pending buffer. Collect BEFORE its
    // query pools can be reset by this new generation's GPU commands.
    {FScopeLock Lock(&Mutex);auto Found=Buffers.find(Cmd);if(Found!=Buffers.end()){Resolve(Found->second,true);Found->second.passes.clear();Found->second.submitted=false;Found->second.inside=false;Found->second.ordinal=0;Found->second.secondary={};Found->second.record_epoch=Enabled.load()?Epoch:0;}}
    const auto Result=Next_vkBeginCommandBuffer(Cmd,Info);
    if(Result==VK_SUCCESS&&Enabled.load()) {FScopeLock Lock(&Mutex);auto& B=Buffers[Cmd];B.primary=Info&&!(Info->flags&VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);B.record_epoch=Epoch;B.secondary={};}
    return Result;
}
VKAPI_ATTR void VKAPI_CALL FreeBuffers(VkDevice D,VkCommandPool Pool,uint32_t Count,const VkCommandBuffer* Cmds) {
    {FScopeLock Lock(&Mutex);if(D==Device)for(uint32 I=0;I<Count;++I){auto F=Buffers.find(Cmds[I]);if(F!=Buffers.end()){Resolve(F->second,true);for(auto P:F->second.pools)DestroyPool(Device,P,nullptr);Buffers.erase(F);}}}
    Next_vkFreeCommandBuffers(D,Pool,Count,Cmds);
}
VKAPI_ATTR VkResult VKAPI_CALL AllocateBuffers(VkDevice D,const VkCommandBufferAllocateInfo* Info,VkCommandBuffer* Cmds) {
    const auto Result=Next_vkAllocateCommandBuffers(D,Info,Cmds);
    if(Result==VK_SUCCESS&&D==Device){FScopeLock Lock(&Mutex);for(uint32 I=0;I<Info->commandBufferCount;++I){auto& B=Buffers[Cmds[I]];B.owner=Info->commandPool;B.primary=Info->level==VK_COMMAND_BUFFER_LEVEL_PRIMARY;}}
    return Result;
}
VKAPI_ATTR void VKAPI_CALL DestroyCommandPool(VkDevice D,VkCommandPool Pool,const VkAllocationCallbacks* Alloc) {
    {FScopeLock Lock(&Mutex);if(D==Device)for(auto I=Buffers.begin();I!=Buffers.end();){if(I->second.owner==Pool){Resolve(I->second,true);for(auto P:I->second.pools)DestroyPool(Device,P,nullptr);I=Buffers.erase(I);}else ++I;}}
    Next_vkDestroyCommandPool(D,Pool,Alloc);
}
VKAPI_ATTR void VKAPI_CALL Begin(VkCommandBuffer Cmd,const VkRenderPassBeginInfo* Info,VkSubpassContents Contents){BeginPass(Cmd,Info);Next_vkCmdBeginRenderPass(Cmd,Info,Contents);}
VKAPI_ATTR void VKAPI_CALL Begin2(VkCommandBuffer Cmd,const VkRenderPassBeginInfo* Info,const VkSubpassBeginInfo* Subpass){BeginPass(Cmd,Info);Next_vkCmdBeginRenderPass2KHR(Cmd,Info,Subpass);}
VKAPI_ATTR void VKAPI_CALL End(VkCommandBuffer Cmd){Next_vkCmdEndRenderPass(Cmd);EndPass(Cmd);}
VKAPI_ATTR void VKAPI_CALL End2(VkCommandBuffer Cmd,const VkSubpassEndInfo* Subpass){Next_vkCmdEndRenderPass2KHR(Cmd,Subpass);EndPass(Cmd);}
VKAPI_ATTR void VKAPI_CALL Subpass(VkCommandBuffer Cmd,VkSubpassContents Contents){Next_vkCmdNextSubpass(Cmd,Contents);FScopeLock Lock(&Mutex);auto F=Buffers.find(Cmd);if(F!=Buffers.end()&&F->second.inside){auto& P=F->second.passes.back();++P.subpasses;P.work.emplace_back();}}
// CPU-side command metadata only: no resource reads or extra GPU queries.
cyinsights_quest::PassWork* WorkOf(VkCommandBuffer Cmd){auto F=Buffers.find(Cmd);if(F==Buffers.end()||F->second.record_epoch!=Epoch)return nullptr;auto& B=F->second;if(!B.primary)return &B.secondary;if(B.inside&&!B.passes.empty())return &B.passes.back().work.back();return nullptr;}
void CountDraw(VkCommandBuffer Cmd,uint32 Count,uint32 Instances,bool Indexed,bool Indirect){if(!Enabled.load(std::memory_order_relaxed))return;FScopeLock Lock(&Mutex);if(auto* W=WorkOf(Cmd)){if(Indirect)W->Indirect(Count);else W->Draw(Count,Instances,Indexed);}}
VKAPI_ATTR void VKAPI_CALL Draw(VkCommandBuffer Cmd,uint32 Vertices,uint32 Instances,uint32 FirstVertex,uint32 FirstInstance){Next_vkCmdDraw(Cmd,Vertices,Instances,FirstVertex,FirstInstance);CountDraw(Cmd,Vertices,Instances,false,false);}
VKAPI_ATTR void VKAPI_CALL DrawIndexed(VkCommandBuffer Cmd,uint32 Indices,uint32 Instances,uint32 FirstIndex,int32_t VertexOffset,uint32 FirstInstance){Next_vkCmdDrawIndexed(Cmd,Indices,Instances,FirstIndex,VertexOffset,FirstInstance);CountDraw(Cmd,Indices,Instances,true,false);}
VKAPI_ATTR void VKAPI_CALL DrawIndirect(VkCommandBuffer Cmd,VkBuffer Buffer,VkDeviceSize Offset,uint32 Count,uint32 Stride){Next_vkCmdDrawIndirect(Cmd,Buffer,Offset,Count,Stride);CountDraw(Cmd,Count,0,false,true);}
VKAPI_ATTR void VKAPI_CALL DrawIndexedIndirect(VkCommandBuffer Cmd,VkBuffer Buffer,VkDeviceSize Offset,uint32 Count,uint32 Stride){Next_vkCmdDrawIndexedIndirect(Cmd,Buffer,Offset,Count,Stride);CountDraw(Cmd,Count,0,true,true);}
VKAPI_ATTR void VKAPI_CALL Execute(VkCommandBuffer Cmd,uint32 Count,const VkCommandBuffer* Commands){Next_vkCmdExecuteCommands(Cmd,Count,Commands);if(!Enabled.load(std::memory_order_relaxed))return;FScopeLock Lock(&Mutex);auto* W=WorkOf(Cmd);if(!W)return;for(uint32 I=0;I<Count;++I){auto F=Buffers.find(Commands[I]);if(F!=Buffers.end()&&!F->second.primary&&F->second.record_epoch==Epoch)W->Merge(F->second.secondary);else W->flags|=cyinsights_quest::PartialSecondary;}}
void Submitted(VkQueue Queue,VkFence Fence,VkCommandBuffer Cmd,uint64 Serial) {
    auto F=Buffers.find(Cmd);if(F==Buffers.end()||F->second.passes.empty())return;
    auto& B=F->second;
    if(Queue!=Graphics){for(const auto& P:B.passes)if(P.epoch==Epoch)++DroppedCount;B.passes.clear();return;}
    B.submitted=true;B.fence=Fence;B.submission=Serial;
}
VKAPI_ATTR VkResult VKAPI_CALL Submit(VkQueue Queue,uint32_t Count,const VkSubmitInfo* Infos,VkFence Fence) {
    // Lock across the real submission and registration so polling cannot race
    // a completed fence that has not yet been associated with its buffers.
    FScopeLock Lock(&Mutex);const auto Result=Next_vkQueueSubmit(Queue,Count,Infos,Fence);
    if(Result==VK_SUCCESS){const auto Serial=++Submission;for(uint32 I=0;I<Count;++I)for(uint32 J=0;J<Infos[I].commandBufferCount;++J)Submitted(Queue,Fence,Infos[I].pCommandBuffers[J],Serial);}
    return Result;
}
VKAPI_ATTR VkResult VKAPI_CALL Submit2(VkQueue Queue,uint32_t Count,const VkSubmitInfo2* Infos,VkFence Fence) {
    FScopeLock Lock(&Mutex);const auto Result=Next_vkQueueSubmit2KHR(Queue,Count,Infos,Fence);
    if(Result==VK_SUCCESS){const auto Serial=++Submission;for(uint32 I=0;I<Count;++I)for(uint32 J=0;J<Infos[I].commandBufferInfoCount;++J)Submitted(Queue,Fence,Infos[I].pCommandBufferInfos[J].commandBuffer,Serial);}
    return Result;
}
void RetireFence(VkDevice D,VkFence Fence){if(D!=Device)return;for(auto& E:Buffers)if(E.second.fence==Fence){Resolve(E.second);E.second.fence=VK_NULL_HANDLE;}}
VKAPI_ATTR VkResult VKAPI_CALL ResetFences(VkDevice D,uint32_t Count,const VkFence* Fences){FScopeLock Lock(&Mutex);for(uint32 I=0;I<Count;++I)RetireFence(D,Fences[I]);return Next_vkResetFences(D,Count,Fences);}
VKAPI_ATTR void VKAPI_CALL DestroyFence(VkDevice D,VkFence Fence,const VkAllocationCallbacks* Alloc){FScopeLock Lock(&Mutex);RetireFence(D,Fence);Next_vkDestroyFence(D,Fence,Alloc);}
template<class T>bool Install(T& Slot,T Wrapper,T& Next){if(Slot==Wrapper)return Next!=nullptr;if(!Slot||Next)return false;Next=Slot;Slot=Wrapper;return true;}
template<class T>void Remove(T& Slot,T Wrapper,T Next){if(Slot==Wrapper)Slot=Next;}
void RemoveHooks() {
#define CY_REMOVE(Name,Wrapper) Remove(VulkanDynamicAPI::Name,Wrapper,Next_##Name);
CY_REMOVE(vkBeginCommandBuffer,BeginBuffer) CY_REMOVE(vkFreeCommandBuffers,FreeBuffers)
CY_REMOVE(vkAllocateCommandBuffers,AllocateBuffers) CY_REMOVE(vkDestroyCommandPool,DestroyCommandPool)
CY_REMOVE(vkCmdBeginRenderPass,Begin) CY_REMOVE(vkCmdBeginRenderPass2KHR,Begin2)
CY_REMOVE(vkCmdEndRenderPass,End) CY_REMOVE(vkCmdEndRenderPass2KHR,End2) CY_REMOVE(vkCmdNextSubpass,Subpass)
CY_REMOVE(vkQueueSubmit,Submit) CY_REMOVE(vkQueueSubmit2KHR,Submit2)
CY_REMOVE(vkResetFences,ResetFences) CY_REMOVE(vkDestroyFence,DestroyFence)
CY_REMOVE(vkCmdDraw,Draw) CY_REMOVE(vkCmdDrawIndexed,DrawIndexed)
CY_REMOVE(vkCmdDrawIndirect,DrawIndirect) CY_REMOVE(vkCmdDrawIndexedIndirect,DrawIndexedIndirect) CY_REMOVE(vkCmdExecuteCommands,Execute)
#undef CY_REMOVE
}
}
bool CyInsightsVulkanPasses::Enable() {
    Disable();bool Installed=false;
    ENQUEUE_RENDER_COMMAND(CyInsightsInstallVulkanPasses)([&Installed](FRHICommandListImmediate& Commands){
        Commands.ImmediateFlush(EImmediateFlushType::FlushRHIThread);FScopeLock Lock(&Mutex);
        auto* RHI=GetIVulkanDynamicRHI();
        if(Device&&Device!=RHI->RHIGetVkDevice()){Reason=TEXT("Vulkan device changed; restart required");return;}
        Device=RHI->RHIGetVkDevice();Graphics=RHI->RHIGetGraphicsVkQueue();
        // Match Unreal's raw-query conversion exactly, including integer Hz.
        VkPhysicalDeviceProperties Props{};
        auto GetProps=(PFN_vkGetPhysicalDeviceProperties)RHI->RHIGetVkInstanceProcAddr("vkGetPhysicalDeviceProperties");
        auto GetFamilies=(PFN_vkGetPhysicalDeviceQueueFamilyProperties)RHI->RHIGetVkInstanceProcAddr("vkGetPhysicalDeviceQueueFamilyProperties");
        if(!GetProps||!GetFamilies){Reason=TEXT("Vulkan physical-device functions unavailable");return;}
        GetProps(RHI->RHIGetVkPhysicalDevice(),&Props);
        uint32 Count=0;GetFamilies(RHI->RHIGetVkPhysicalDevice(),&Count,nullptr);std::vector<VkQueueFamilyProperties> Families(Count);GetFamilies(RHI->RHIGetVkPhysicalDevice(),&Count,Families.data());
        const auto Family=RHI->RHIGetGraphicsQueueFamilyIndex();
        if(Family>=Count||!Families[Family].timestampValidBits||Families[Family].timestampValidBits>64||Props.limits.timestampPeriod<=0){Reason=TEXT("Graphics queue timestamp queries unavailable");return;}
        TimestampBits=Families[Family].timestampValidBits;
        Frequency=double(uint64(1e9/double(Props.limits.timestampPeriod)));
#define CY_PROC(Name,Var) Var=(PFN_##Name)RHI->RHIGetVkDeviceProcAddr(#Name);if(!Var){Reason=TEXT("Vulkan query functions unavailable");return;}
        CY_PROC(vkCreateQueryPool,CreatePool) CY_PROC(vkDestroyQueryPool,DestroyPool)
        CY_PROC(vkCmdResetQueryPool,ResetPool) CY_PROC(vkCmdWriteTimestamp,Timestamp)
        CY_PROC(vkGetQueryPoolResults,Results) CY_PROC(vkGetFenceStatus,FenceStatus) CY_PROC(vkDeviceWaitIdle,DeviceIdle)
#undef CY_PROC
        Calibrated=nullptr;
        for(const auto& Extension:RHI->RHIGetLoadedDeviceExtensions())if(Extension==VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME)
            Calibrated=(PFN_vkGetCalibratedTimestampsEXT)RHI->RHIGetVkDeviceProcAddr("vkGetCalibratedTimestampsEXT");
        bool Ok=true;
#define CY_INSTALL(Name,Wrapper) Ok=Install(VulkanDynamicAPI::Name,Wrapper,Next_##Name)&&Ok;
        CY_INSTALL(vkBeginCommandBuffer,BeginBuffer) CY_INSTALL(vkFreeCommandBuffers,FreeBuffers)
        CY_INSTALL(vkAllocateCommandBuffers,AllocateBuffers) CY_INSTALL(vkDestroyCommandPool,DestroyCommandPool)
        CY_INSTALL(vkCmdBeginRenderPass,Begin) CY_INSTALL(vkCmdEndRenderPass,End)
        CY_INSTALL(vkCmdNextSubpass,Subpass) CY_INSTALL(vkQueueSubmit,Submit)
        CY_INSTALL(vkResetFences,ResetFences) CY_INSTALL(vkDestroyFence,DestroyFence)
        CY_INSTALL(vkCmdDraw,Draw) CY_INSTALL(vkCmdDrawIndexed,DrawIndexed)
        CY_INSTALL(vkCmdDrawIndirect,DrawIndirect) CY_INSTALL(vkCmdDrawIndexedIndirect,DrawIndexedIndirect) CY_INSTALL(vkCmdExecuteCommands,Execute)
        // Install both sides together: Unreal's render-pass-2 path never calls
        // vkCmdEndRenderPass, so intercepting just its begin loses every cost.
        if(VulkanDynamicAPI::vkCmdBeginRenderPass2KHR||VulkanDynamicAPI::vkCmdEndRenderPass2KHR){CY_INSTALL(vkCmdBeginRenderPass2KHR,Begin2) CY_INSTALL(vkCmdEndRenderPass2KHR,End2)}
        if(VulkanDynamicAPI::vkQueueSubmit2KHR){CY_INSTALL(vkQueueSubmit2KHR,Submit2)}
#undef CY_INSTALL
        if(!Ok){RemoveHooks();Reason=TEXT("Vulkan pass dispatch unavailable or owned by another interceptor");return;}
        ++Epoch;ResolvedCount=DroppedCount=0;LastClock=0;Installed=true;
        Reason=TEXT("Graphics render-pass boundary timestamps; delayed fence-verified reads; engine clock alignment remains approximate. Subpass draw/indirect arguments are recorded without resource reads; individual subpass GPU costs remain unavailable");
        Enabled.store(true);
    });
    FlushRenderingCommands();
    if(Installed&&!PollHandle.IsValid())PollHandle=FCoreDelegates::OnEndFrame.AddStatic(Poll);
    return Installed;
}
void CyInsightsVulkanPasses::Disable(){FScopeLock Lock(&Mutex);PollLocked();Enabled.store(false);}
void CyInsightsVulkanPasses::Shutdown() {
    Disable();FCoreDelegates::OnEndFrame.Remove(PollHandle);PollHandle.Reset();
    if(!Next_vkBeginCommandBuffer||!GDynamicRHI)return;
    ENQUEUE_RENDER_COMMAND(CyInsightsRemoveVulkanPasses)([](FRHICommandListImmediate& Commands){
        Commands.ImmediateFlush(EImmediateFlushType::FlushRHIThread);FScopeLock Lock(&Mutex);
        RemoveHooks();
        // Only teardown waits for GPU retirement. Capture start/stop never do.
        if(Device&&GDynamicRHI->GetInterfaceType()==ERHIInterfaceType::Vulkan&&GetIVulkanDynamicRHI()->RHIGetVkDevice()==Device&&DeviceIdle(Device)==VK_SUCCESS){for(auto& E:Buffers)for(auto P:E.second.pools)DestroyPool(Device,P,nullptr);}
        Buffers.clear();Device=VK_NULL_HANDLE;
    });FlushRenderingCommands();
}
FString CyInsightsVulkanPasses::Status(){FScopeLock Lock(&Mutex);return Reason;}
uint64 CyInsightsVulkanPasses::Resolved(){FScopeLock Lock(&Mutex);return ResolvedCount;}
uint64 CyInsightsVulkanPasses::Dropped(){FScopeLock Lock(&Mutex);return DroppedCount;}
uint64 CyInsightsVulkanPasses::Pending(){FScopeLock Lock(&Mutex);uint64 N=0;for(auto& E:Buffers)for(auto& P:E.second.passes)if(P.epoch==Epoch)++N;return N;}
#else
bool CyInsightsVulkanPasses::Enable(){return false;}
void CyInsightsVulkanPasses::Disable(){}
void CyInsightsVulkanPasses::Shutdown(){}
FString CyInsightsVulkanPasses::Status(){return TEXT("Vulkan pass collector unavailable in this engine/platform build");}
uint64 CyInsightsVulkanPasses::Resolved(){return 0;}
uint64 CyInsightsVulkanPasses::Dropped(){return 0;}
uint64 CyInsightsVulkanPasses::Pending(){return 0;}
#endif

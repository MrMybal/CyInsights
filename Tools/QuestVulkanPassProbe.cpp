// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
// Developer-only headless hardware probe, never launched by capture/plugin.
#include <vulkan/vulkan.h>
#include "../Plugins/CyInsights/Source/CyInsights/Private/QuestPassClock.h"
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>
#if CY_PERFETTO_WORKLOAD
#include <chrono>
#include <thread>
#include "MetaProbeShaders.h"
#endif
#if CY_TEST_COLLECTOR
#include "../Tests/QuestVulkanShim/Dispatch.h"
#endif
static void CheckResult(VkResult r,int line){if(r!=VK_SUCCESS)throw std::runtime_error("Vulkan operation failed: "+std::to_string(r)+" at line "+std::to_string(line));}
#define Check(r) CheckResult(r,__LINE__)
int main(){
 try {
    VkInstance instance{};VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};application.apiVersion=VK_API_VERSION_1_2;VkInstanceCreateInfo ic{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ic.pApplicationInfo=&application;Check(vkCreateInstance(&ic,nullptr,&instance));
    uint32_t n=0;Check(vkEnumeratePhysicalDevices(instance,&n,nullptr));std::vector<VkPhysicalDevice> physical(n);Check(vkEnumeratePhysicalDevices(instance,&n,physical.data()));if(!n)return 2;
    auto gpu=physical[0];VkPhysicalDeviceProperties props{};vkGetPhysicalDeviceProperties(gpu,&props);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu,&n,nullptr);std::vector<VkQueueFamilyProperties> families(n);vkGetPhysicalDeviceQueueFamilyProperties(gpu,&n,families.data());
    uint32_t family=0;while(family<n&&!(families[family].queueFlags&VK_QUEUE_GRAPHICS_BIT))++family;if(family==n)return 2;
    float priorities[2]={1,1};VkDeviceQueueCreateInfo qc{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qc.queueFamilyIndex=family;qc.queueCount=families[family].queueCount>=2?2:1;qc.pQueuePriorities=priorities;
    VkDeviceCreateInfo dc{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};dc.queueCreateInfoCount=1;dc.pQueueCreateInfos=&qc;
#if CY_TEST_COLLECTOR
    VkPhysicalDeviceTimelineSemaphoreFeatures timelineFeature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES};timelineFeature.timelineSemaphore=VK_TRUE;dc.pNext=&timelineFeature;
#endif
    VkDevice device{};Check(vkCreateDevice(gpu,&dc,nullptr,&device));VkQueue queue{};vkGetDeviceQueue(device,family,0,&queue);
#if CY_TEST_COLLECTOR
    MockRHI={instance,device,gpu,queue,family};
    // The Meta UE 5.5 render-pass-2 path uses KHR dispatch slots. Vulkan 1.2
    // promotes those commands; point the test slots at the real core functions.
    vkCmdBeginRenderPass2KHR=(PFN_vkCmdBeginRenderPass2KHR)vkGetDeviceProcAddr(device,"vkCmdBeginRenderPass2");
    vkCmdEndRenderPass2KHR=(PFN_vkCmdEndRenderPass2KHR)vkGetDeviceProcAddr(device,"vkCmdEndRenderPass2");
    if(!vkCmdBeginRenderPass2KHR||!vkCmdEndRenderPass2KHR)throw std::runtime_error("Render-pass-2 commands unavailable");
    if(!CyInsightsVulkanPasses::Enable())throw std::runtime_error("Actual collector did not start");
#endif
    VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ii.imageType=VK_IMAGE_TYPE_2D;ii.format=VK_FORMAT_R8G8B8A8_UNORM;ii.extent={1536,1536,1};ii.mipLevels=ii.arrayLayers=1;ii.samples=VK_SAMPLE_COUNT_1_BIT;ii.tiling=VK_IMAGE_TILING_OPTIMAL;ii.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;VkImage image{};Check(vkCreateImage(device,&ii,nullptr,&image));
    VkMemoryRequirements req{};vkGetImageMemoryRequirements(device,image,&req);VkPhysicalDeviceMemoryProperties memory{};vkGetPhysicalDeviceMemoryProperties(gpu,&memory);
    uint32_t type=0;while(type<memory.memoryTypeCount&&(!(req.memoryTypeBits&(1u<<type))||!(memory.memoryTypes[type].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)))++type;if(type==memory.memoryTypeCount)return 2;
    VkMemoryAllocateInfo ma{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ma.allocationSize=req.size;ma.memoryTypeIndex=type;VkDeviceMemory allocation{};Check(vkAllocateMemory(device,&ma,nullptr,&allocation));Check(vkBindImageMemory(device,image,allocation,0));
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=image;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=ii.format;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};VkImageView view{};Check(vkCreateImageView(device,&vi,nullptr,&view));
    VkAttachmentDescription attachment{};attachment.format=ii.format;attachment.samples=VK_SAMPLE_COUNT_1_BIT;attachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;attachment.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachment.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;attachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;attachment.finalLayout=VK_IMAGE_LAYOUT_GENERAL;
    VkAttachmentReference ref{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};VkSubpassDescription subpass{};subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;subpass.colorAttachmentCount=1;subpass.pColorAttachments=&ref;
    VkRenderPassCreateInfo ri{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};ri.attachmentCount=1;ri.pAttachments=&attachment;ri.subpassCount=1;ri.pSubpasses=&subpass;VkRenderPass renderpass{};Check(vkCreateRenderPass(device,&ri,nullptr,&renderpass));
#if CY_PERFETTO_WORKLOAD
    VkPipelineLayoutCreateInfo plc{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};VkPipelineLayout layout{};Check(vkCreatePipelineLayout(device,&plc,nullptr,&layout));
    VkShaderModule shaders[2]{};VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sm.codeSize=sizeof(MetaVertex);sm.pCode=MetaVertex;Check(vkCreateShaderModule(device,&sm,nullptr,&shaders[0]));sm.codeSize=sizeof(MetaFragment);sm.pCode=MetaFragment;Check(vkCreateShaderModule(device,&sm,nullptr,&shaders[1]));
    VkPipelineShaderStageCreateInfo stages[2]{};for(int i=0;i<2;++i){stages[i].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;stages[i].stage=i?VK_SHADER_STAGE_FRAGMENT_BIT:VK_SHADER_STAGE_VERTEX_BIT;stages[i].module=shaders[i];stages[i].pName="main";}
    VkPipelineVertexInputStateCreateInfo vertex{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkViewport viewport{0,0,1536,1536,0,1};VkRect2D scissor{{0,0},{1536,1536}};VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vp.viewportCount=vp.scissorCount=1;vp.pViewports=&viewport;vp.pScissors=&scissor;
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};raster.polygonMode=VK_POLYGON_MODE_FILL;raster.lineWidth=1;VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState color{};color.colorWriteMask=15;VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};blend.attachmentCount=1;blend.pAttachments=&color;
    VkGraphicsPipelineCreateInfo graphics{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};graphics.stageCount=2;graphics.pStages=stages;graphics.pVertexInputState=&vertex;graphics.pInputAssemblyState=&assembly;graphics.pViewportState=&vp;graphics.pRasterizationState=&raster;graphics.pMultisampleState=&ms;graphics.pColorBlendState=&blend;graphics.layout=layout;graphics.renderPass=renderpass;
    VkPipeline pipeline{};Check(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&graphics,nullptr,&pipeline));
#endif
    VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fi.renderPass=renderpass;fi.attachmentCount=1;fi.pAttachments=&view;fi.width=fi.height=1536;fi.layers=1;VkFramebuffer framebuffer{};Check(vkCreateFramebuffer(device,&fi,nullptr,&framebuffer));
    VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pi.queueFamilyIndex=family;pi.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;VkCommandPool pool{};Check(vkCreateCommandPool(device,&pi,nullptr,&pool));VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;VkCommandBuffer cmd{};Check(vkAllocateCommandBuffers(device,&ca,&cmd));
    VkQueryPoolCreateInfo qi{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};qi.queryType=VK_QUERY_TYPE_TIMESTAMP;qi.queryCount=8;VkQueryPool queries{};Check(vkCreateQueryPool(device,&qi,nullptr,&queries));
    VkCommandBufferBeginInfo cb{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};Check(vkBeginCommandBuffer(cmd,&cb));vkCmdResetQueryPool(cmd,queries,0,8);
    for(uint32_t pass=0;pass<2;++pass){
        vkCmdWriteTimestamp(cmd,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,queries,pass*4);
        VkClearValue clear{};clear.color.float32[3]=1;VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};begin.renderPass=renderpass;begin.framebuffer=framebuffer;begin.renderArea.extent={1536,1536};begin.clearValueCount=1;begin.pClearValues=&clear;
#if CY_TEST_COLLECTOR
        if(pass){VkSubpassBeginInfo subpassBegin{VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO};subpassBegin.contents=VK_SUBPASS_CONTENTS_INLINE;vkCmdBeginRenderPass2KHR(cmd,&begin,&subpassBegin);}
        else
#endif
        vkCmdBeginRenderPass(cmd,&begin,VK_SUBPASS_CONTENTS_INLINE);
        vkCmdWriteTimestamp(cmd,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,queries,pass*4+1);
        for(uint32_t i=0;i<64;++i){VkClearAttachment a{};a.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;a.clearValue.color.float32[0]=float(i)/64;a.clearValue.color.float32[3]=1;VkClearRect rect{};rect.rect.offset={int32_t(i%8)*128,int32_t(i/8)*128};rect.rect.extent={128,128};rect.layerCount=1;vkCmdClearAttachments(cmd,1,&a,1,&rect);}
        vkCmdWriteTimestamp(cmd,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,queries,pass*4+2);
#if CY_TEST_COLLECTOR
        if(pass){VkSubpassEndInfo subpassEnd{VK_STRUCTURE_TYPE_SUBPASS_END_INFO};vkCmdEndRenderPass2KHR(cmd,&subpassEnd);}
        else
#endif
        vkCmdEndRenderPass(cmd);
        vkCmdWriteTimestamp(cmd,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,queries,pass*4+3);
        if(!pass){VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};barrier.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,0,1,&barrier,0,nullptr,0,nullptr);}
    }
    Check(vkEndCommandBuffer(cmd));VkFenceCreateInfo fc{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};VkFence fence{};Check(vkCreateFence(device,&fc,nullptr,&fence));VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};si.commandBufferCount=1;si.pCommandBuffers=&cmd;Check(vkQueueSubmit(queue,1,&si,fence));
    // The standalone test waits; the actual collector exclusively polls fences.
    Check(vkWaitForFences(device,1,&fence,VK_TRUE,5000000000ull));
    struct Reading{uint64_t value,available;};Reading results[8]{};Check(vkGetQueryPoolResults(device,queries,0,8,sizeof(results),results,sizeof(Reading),VK_QUERY_RESULT_64_BIT|VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
    std::printf("Device: %s; timestamp bits: %u; period: %.6f ns\n",props.deviceName,families[family].timestampValidBits,props.limits.timestampPeriod);
    bool valid=true;for(uint32_t pass=0;pass<2;++pass){uint64_t outside=0,inside=0;valid&=cyinsights_quest::TimestampSpan(results[pass*4].value,results[pass*4+3].value,families[family].timestampValidBits,outside);valid&=cyinsights_quest::TimestampSpan(results[pass*4+1].value,results[pass*4+2].value,families[family].timestampValidBits,inside);for(uint32_t i=0;i<4;++i)valid&=results[pass*4+i].available!=0;std::printf("Pass %u: boundary %.6f ms; inner %.6f ms\n",pass+1,double(outside)*props.limits.timestampPeriod/1e6,double(inside)*props.limits.timestampPeriod/1e6);valid&=outside>0;}
#if CY_TEST_COLLECTOR
    CyInsightsVulkanPasses::Disable();
    valid&=CyInsightsVulkanPasses::Resolved()==2&&CyInsightsVulkanPasses::Pending()==0&&CyInsightsVulkanPasses::Dropped()==0;
    std::printf("Actual collector: %llu resolved, %llu pending, %llu dropped\n",(unsigned long long)CyInsightsVulkanPasses::Resolved(),(unsigned long long)CyInsightsVulkanPasses::Pending(),(unsigned long long)CyInsightsVulkanPasses::Dropped());
    const bool bothApis=CyInsightsVulkanPasses::Resolved()==2&&MockTraceRecord::records.size()==4;
    valid&=bothApis;std::printf("Legacy and render-pass-2 paths both resolve: %s\n",bothApis?"PASS":"FAIL");
    for(const auto& record:MockTraceRecord::records){uint64_t a=0,b=0;for(const auto& field:record){if(std::string(field.name)=="BeginUs")a=field.value;if(std::string(field.name)=="EndUs")b=field.value;}std::printf("Collector interval %.6f ms\n",double(b-a)/1000.);valid&=a>0&&b>a;}
    if(qc.queueCount>=2) {
        // Reused query pools still contain the preceding generation's available
        // values. Block the new submission to prove those are never consumed.
        Check(vkResetFences(device,1,&fence));if(!CyInsightsVulkanPasses::Enable())throw std::runtime_error("Repeated capture failed");
        Check(vkBeginCommandBuffer(cmd,&cb));VkClearValue clear{};VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};begin.renderPass=renderpass;begin.framebuffer=framebuffer;begin.renderArea.extent={1536,1536};begin.clearValueCount=1;begin.pClearValues=&clear;vkCmdBeginRenderPass(cmd,&begin,VK_SUBPASS_CONTENTS_INLINE);vkCmdEndRenderPass(cmd);Check(vkEndCommandBuffer(cmd));
        VkSemaphoreTypeCreateInfo kind{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};kind.semaphoreType=VK_SEMAPHORE_TYPE_TIMELINE;VkSemaphoreCreateInfo sc{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};sc.pNext=&kind;VkSemaphore blocked{};Check(vkCreateSemaphore(device,&sc,nullptr,&blocked));VkPipelineStageFlags wait=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;uint64_t value=1;VkTimelineSemaphoreSubmitInfo timeline{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};timeline.waitSemaphoreValueCount=1;timeline.pWaitSemaphoreValues=&value;si.pNext=&timeline;si.waitSemaphoreCount=1;si.pWaitSemaphores=&blocked;si.pWaitDstStageMask=&wait;Check(vkQueueSubmit(queue,1,&si,fence));
        FCoreDelegates::OnEndFrame.Broadcast();const bool stale=CyInsightsVulkanPasses::Resolved()==0&&CyInsightsVulkanPasses::Pending()==1;valid&=stale;std::printf("Blocked generation rejects stale query values: %s\n",stale?"PASS":"FAIL");
        auto signal=(PFN_vkSignalSemaphore)vkGetDeviceProcAddr(device,"vkSignalSemaphore");if(!signal)throw std::runtime_error("Timeline host signal unavailable");VkSemaphoreSignalInfo signalInfo{VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO};signalInfo.semaphore=blocked;signalInfo.value=1;Check(signal(device,&signalInfo));Check(vkWaitForFences(device,1,&fence,VK_TRUE,5000000000ull));FCoreDelegates::OnEndFrame.Broadcast();const bool fresh=CyInsightsVulkanPasses::Resolved()==1&&CyInsightsVulkanPasses::Pending()==0;valid&=fresh;std::printf("Completed reused generation resolves once: %s\n",fresh?"PASS":"FAIL");CyInsightsVulkanPasses::Disable();vkDestroySemaphore(device,blocked,nullptr);
    }
#endif
#if CY_PERFETTO_WORKLOAD
    // Standalone test workload for a simultaneous system render-stage trace.
    // No game/plugin capture invokes this loop.
    si.pNext=nullptr;si.waitSemaphoreCount=0;si.pWaitSemaphores=nullptr;si.pWaitDstStageMask=nullptr;
    Check(vkBeginCommandBuffer(cmd,&cb));VkClearValue clear{};VkRenderPassBeginInfo workload{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};workload.renderPass=renderpass;workload.framebuffer=framebuffer;workload.renderArea.extent={1536,1536};workload.clearValueCount=1;workload.pClearValues=&clear;
    vkCmdBeginRenderPass(cmd,&workload,VK_SUBPASS_CONTENTS_INLINE);vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);vkCmdDraw(cmd,3,1,0,0);vkCmdEndRenderPass(cmd);Check(vkEndCommandBuffer(cmd));
    for(int frame=0;frame<240;++frame){Check(vkResetFences(device,1,&fence));Check(vkQueueSubmit(queue,1,&si,fence));Check(vkWaitForFences(device,1,&fence,VK_TRUE,5000000000ull));std::this_thread::sleep_for(std::chrono::milliseconds(10));}
#endif
    vkDestroyFence(device,fence,nullptr);vkDestroyCommandPool(device,pool,nullptr);
#if CY_PERFETTO_WORKLOAD
    vkDestroyPipeline(device,pipeline,nullptr);for(auto shader:shaders)vkDestroyShaderModule(device,shader,nullptr);vkDestroyPipelineLayout(device,layout,nullptr);
#endif
#if CY_TEST_COLLECTOR
    CyInsightsVulkanPasses::Shutdown();
#endif
    vkDestroyQueryPool(device,queries,nullptr);vkDestroyFramebuffer(device,framebuffer,nullptr);vkDestroyRenderPass(device,renderpass,nullptr);vkDestroyImageView(device,view,nullptr);vkDestroyImage(device,image,nullptr);vkFreeMemory(device,allocation,nullptr);vkDestroyDevice(device,nullptr);vkDestroyInstance(instance,nullptr);return valid?0:3;
 }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}

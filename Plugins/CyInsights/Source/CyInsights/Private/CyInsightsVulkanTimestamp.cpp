// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#include "CyInsightsVulkanTimestamp.h"
#if CYINSIGHTS_VULKAN_DISPATCH
#include "VulkanThirdParty.h"
#include "RHICommandList.h"
#include "RenderingThread.h"
#include "QuestTimestampDispatch.h"

// Exported by VulkanRHI's loader. No private engine header or binary patch.
namespace VulkanDynamicAPI {
extern VULKANRHI_API PFN_vkCmdWriteTimestamp vkCmdWriteTimestamp;
}
namespace {
cyinsights_quest::TimestampDispatch<PFN_vkCmdWriteTimestamp> Dispatch;
VKAPI_ATTR void VKAPI_CALL WriteTimestamp(VkCommandBuffer Buffer,VkPipelineStageFlagBits Stage,VkQueryPool Pool,uint32_t Query) {
    // Preserve other stages, pools, indices and the engine's delayed readback.
    // No extra query, barrier, flush, resource copy or CPU timestamp is added.
    Stage=Dispatch.Transform(Stage,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    Dispatch.Next()(Buffer,Stage,Pool,Query);
}
}
bool CyInsightsVulkanTimestamp::Enable() {
    bool Installed=false;
    ENQUEUE_RENDER_COMMAND(CyInsightsInstallVulkanTimestamp)([&Installed](FRHICommandListImmediate& Commands){
        // Serialize against RHI command recording. This waits for CPU commands,
        // not GPU completion, and happens once when first enabling this path.
        Commands.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
        Installed=Dispatch.Install(VulkanDynamicAPI::vkCmdWriteTimestamp,WriteTimestamp);
    });
    FlushRenderingCommands();
    Dispatch.SetEnabled(Installed);
    return Installed;
}
void CyInsightsVulkanTimestamp::Disable(){Dispatch.Disable();}
void CyInsightsVulkanTimestamp::Shutdown(){
    Disable();
    if(!Dispatch.Next()||!GDynamicRHI)return;
    ENQUEUE_RENDER_COMMAND(CyInsightsRemoveVulkanTimestamp)([](FRHICommandListImmediate& Commands){
        Commands.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
        // Never overwrite an interceptor installed by another module after us.
        Dispatch.Remove(VulkanDynamicAPI::vkCmdWriteTimestamp,WriteTimestamp);
    });
    FlushRenderingCommands();
}
#else
bool CyInsightsVulkanTimestamp::Enable(){return false;}
void CyInsightsVulkanTimestamp::Disable(){}
void CyInsightsVulkanTimestamp::Shutdown(){}
#endif

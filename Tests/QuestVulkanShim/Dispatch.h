// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "../../Plugins/CyInsights/Source/CyInsights/Private/CyInsightsVulkanPasses.h"
namespace VulkanDynamicAPI {
#define MOCK_DISPATCH(Name) PFN_##Name Name=::Name;
MOCK_DISPATCH(vkBeginCommandBuffer) MOCK_DISPATCH(vkFreeCommandBuffers)
MOCK_DISPATCH(vkAllocateCommandBuffers) MOCK_DISPATCH(vkDestroyCommandPool)
MOCK_DISPATCH(vkCmdBeginRenderPass)
PFN_vkCmdBeginRenderPass2KHR vkCmdBeginRenderPass2KHR=nullptr;
#ifndef CYINSIGHTS_VULKAN_END_PASS2
#define CYINSIGHTS_VULKAN_END_PASS2 1
#endif
#if CYINSIGHTS_VULKAN_END_PASS2
PFN_vkCmdEndRenderPass2KHR vkCmdEndRenderPass2KHR=nullptr;
#endif
MOCK_DISPATCH(vkCmdEndRenderPass) MOCK_DISPATCH(vkCmdNextSubpass)
MOCK_DISPATCH(vkQueueSubmit)
PFN_vkQueueSubmit2KHR vkQueueSubmit2KHR=nullptr;
MOCK_DISPATCH(vkResetFences) MOCK_DISPATCH(vkDestroyFence)
MOCK_DISPATCH(vkCmdDraw) MOCK_DISPATCH(vkCmdDrawIndexed)
MOCK_DISPATCH(vkCmdDrawIndirect) MOCK_DISPATCH(vkCmdDrawIndexedIndirect) MOCK_DISPATCH(vkCmdExecuteCommands)
#undef MOCK_DISPATCH
}
#define vkBeginCommandBuffer VulkanDynamicAPI::vkBeginCommandBuffer
#define vkFreeCommandBuffers VulkanDynamicAPI::vkFreeCommandBuffers
#define vkAllocateCommandBuffers VulkanDynamicAPI::vkAllocateCommandBuffers
#define vkDestroyCommandPool VulkanDynamicAPI::vkDestroyCommandPool
#define vkCmdBeginRenderPass VulkanDynamicAPI::vkCmdBeginRenderPass
#define vkCmdBeginRenderPass2KHR VulkanDynamicAPI::vkCmdBeginRenderPass2KHR
#define vkCmdEndRenderPass VulkanDynamicAPI::vkCmdEndRenderPass
#if CYINSIGHTS_VULKAN_END_PASS2
#define vkCmdEndRenderPass2KHR VulkanDynamicAPI::vkCmdEndRenderPass2KHR
#endif
#define vkCmdNextSubpass VulkanDynamicAPI::vkCmdNextSubpass
#define vkQueueSubmit VulkanDynamicAPI::vkQueueSubmit
#define vkQueueSubmit2KHR VulkanDynamicAPI::vkQueueSubmit2KHR
#define vkResetFences VulkanDynamicAPI::vkResetFences
#define vkDestroyFence VulkanDynamicAPI::vkDestroyFence

#define vkCmdDraw VulkanDynamicAPI::vkCmdDraw
#define vkCmdDrawIndexed VulkanDynamicAPI::vkCmdDrawIndexed
#define vkCmdDrawIndirect VulkanDynamicAPI::vkCmdDrawIndirect
#define vkCmdDrawIndexedIndirect VulkanDynamicAPI::vkCmdDrawIndexedIndirect
#define vkCmdExecuteCommands VulkanDynamicAPI::vkCmdExecuteCommands

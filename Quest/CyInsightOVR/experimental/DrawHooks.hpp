// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
// Included after Device and the original hooks. Calls into our HUD use Next()
// directly, so the diagnostic renderer's own draws are never counted.
#pragma once
namespace {
void GeometryDraw(const std::shared_ptr<Device>& device,VkCommandBuffer cb,uint32_t count,uint32_t instances,bool indexed){
    std::lock_guard<std::mutex> guard(device->mutex);auto& state=device->geometry[cb];
    auto found=device->geometryPipelines.find(state.pipeline);GeometryPipeline info;if(found!=device->geometryPipelines.end())info=found->second;
    int topology=info.dynamicTopology?state.topology:info.topology;
    bool restart=info.dynamicRestart?state.restart!=0:info.restart;
    device->drawBook.Draw(reinterpret_cast<uintptr_t>(cb),cydraw::Direct(topology,restart,indexed,count,instances));
}
void GeometryIndirect(const std::shared_ptr<Device>& device,VkCommandBuffer cb,uint32_t count,bool gpuCount=false){
    std::lock_guard<std::mutex> guard(device->mutex);device->drawBook.Draw(reinterpret_cast<uintptr_t>(cb),cydraw::Indirect(count,gpuCount));
}
cydraw::Counts GeometrySubmit(const std::shared_ptr<Device>& device,const std::vector<cydraw::Handle>& buffers){
    std::lock_guard<std::mutex> guard(device->mutex);return device->drawBook.Submit(buffers);
}
}
extern "C" {
VKAPI_ATTR VkResult VKAPI_CALL vkCreateGraphicsPipelines(VkDevice handle,VkPipelineCache cache,uint32_t count,const VkGraphicsPipelineCreateInfo* info,const VkAllocationCallbacks* alloc,VkPipeline* output){
    auto device=GetDevice(handle);if(!device)return VK_ERROR_INITIALIZATION_FAILED;
    auto result=Next<PFN_vkCreateGraphicsPipelines>(*device,"vkCreateGraphicsPipelines")(handle,cache,count,info,alloc,output);
    std::lock_guard<std::mutex> guard(device->mutex);
    for(uint32_t i=0;i<count;++i)if(output[i]){
        GeometryPipeline value;if(info[i].pInputAssemblyState){value.topology=info[i].pInputAssemblyState->topology;value.restart=info[i].pInputAssemblyState->primitiveRestartEnable;}
        if(info[i].pDynamicState)for(uint32_t j=0;j<info[i].pDynamicState->dynamicStateCount;++j){auto state=info[i].pDynamicState->pDynamicStates[j];value.dynamicTopology|=state==VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY;value.dynamicRestart|=state==VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE;}
        device->geometryPipelines[output[i]]=value;
    }return result;
}
VKAPI_ATTR void VKAPI_CALL vkDestroyPipeline(VkDevice handle,VkPipeline pipeline,const VkAllocationCallbacks* alloc){auto device=GetDevice(handle);if(!device)return;{std::lock_guard<std::mutex> guard(device->mutex);device->geometryPipelines.erase(pipeline);}Next<PFN_vkDestroyPipeline>(*device,"vkDestroyPipeline")(handle,pipeline,alloc);}
VKAPI_ATTR void VKAPI_CALL vkCmdDraw(VkCommandBuffer cb,uint32_t count,uint32_t instances,uint32_t first,uint32_t firstInstance){auto d=GetDevice(cb);if(!d)return;GeometryDraw(d,cb,count,instances,false);Next<PFN_vkCmdDraw>(*d,"vkCmdDraw")(cb,count,instances,first,firstInstance);}
VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexed(VkCommandBuffer cb,uint32_t count,uint32_t instances,uint32_t first,int32_t vertex,uint32_t firstInstance){auto d=GetDevice(cb);if(!d)return;GeometryDraw(d,cb,count,instances,true);Next<PFN_vkCmdDrawIndexed>(*d,"vkCmdDrawIndexed")(cb,count,instances,first,vertex,firstInstance);}
#define CY_INDIRECT(fn) VKAPI_ATTR void VKAPI_CALL fn(VkCommandBuffer cb,VkBuffer buffer,VkDeviceSize offset,uint32_t count,uint32_t stride){auto d=GetDevice(cb);if(!d)return;GeometryIndirect(d,cb,count);Next<PFN_##fn>(*d,#fn)(cb,buffer,offset,count,stride);}
CY_INDIRECT(vkCmdDrawIndirect)
CY_INDIRECT(vkCmdDrawIndexedIndirect)
CY_INDIRECT(vkCmdDrawMeshTasksIndirectEXT)
CY_INDIRECT(vkCmdDrawMeshTasksIndirectNV)
#undef CY_INDIRECT
#define CY_INDIRECT_COUNT(fn) VKAPI_ATTR void VKAPI_CALL fn(VkCommandBuffer cb,VkBuffer buffer,VkDeviceSize offset,VkBuffer countBuffer,VkDeviceSize countOffset,uint32_t maximum,uint32_t stride){auto d=GetDevice(cb);if(!d)return;GeometryIndirect(d,cb,0,true);Next<PFN_##fn>(*d,#fn)(cb,buffer,offset,countBuffer,countOffset,maximum,stride);}
CY_INDIRECT_COUNT(vkCmdDrawIndirectCount)
CY_INDIRECT_COUNT(vkCmdDrawIndirectCountKHR)
CY_INDIRECT_COUNT(vkCmdDrawIndirectCountAMD)
CY_INDIRECT_COUNT(vkCmdDrawIndexedIndirectCount)
CY_INDIRECT_COUNT(vkCmdDrawIndexedIndirectCountKHR)
CY_INDIRECT_COUNT(vkCmdDrawIndexedIndirectCountAMD)
CY_INDIRECT_COUNT(vkCmdDrawMeshTasksIndirectCountEXT)
CY_INDIRECT_COUNT(vkCmdDrawMeshTasksIndirectCountNV)
#undef CY_INDIRECT_COUNT
#define CY_TOPOLOGY(fn) VKAPI_ATTR void VKAPI_CALL fn(VkCommandBuffer cb,VkPrimitiveTopology value){auto d=GetDevice(cb);if(!d)return;{std::lock_guard<std::mutex> g(d->mutex);d->geometry[cb].topology=value;}Next<PFN_##fn>(*d,#fn)(cb,value);}
CY_TOPOLOGY(vkCmdSetPrimitiveTopology)
CY_TOPOLOGY(vkCmdSetPrimitiveTopologyEXT)
#undef CY_TOPOLOGY
#define CY_RESTART(fn) VKAPI_ATTR void VKAPI_CALL fn(VkCommandBuffer cb,VkBool32 value){auto d=GetDevice(cb);if(!d)return;{std::lock_guard<std::mutex> g(d->mutex);d->geometry[cb].restart=value?1:0;}Next<PFN_##fn>(*d,#fn)(cb,value);}
CY_RESTART(vkCmdSetPrimitiveRestartEnable)
CY_RESTART(vkCmdSetPrimitiveRestartEnableEXT)
#undef CY_RESTART
VKAPI_ATTR void VKAPI_CALL vkCmdExecuteCommands(VkCommandBuffer cb,uint32_t count,const VkCommandBuffer* buffers){auto d=GetDevice(cb);if(!d)return;{std::lock_guard<std::mutex> g(d->mutex);for(uint32_t i=0;i<count;++i)d->drawBook.Execute(reinterpret_cast<uintptr_t>(cb),reinterpret_cast<uintptr_t>(buffers[i]));}Next<PFN_vkCmdExecuteCommands>(*d,"vkCmdExecuteCommands")(cb,count,buffers);}
VKAPI_ATTR VkResult VKAPI_CALL vkResetCommandBuffer(VkCommandBuffer cb,VkCommandBufferResetFlags flags){auto d=GetDevice(cb);if(!d)return VK_ERROR_INITIALIZATION_FAILED;auto result=Next<PFN_vkResetCommandBuffer>(*d,"vkResetCommandBuffer")(cb,flags);if(result==VK_SUCCESS){std::lock_guard<std::mutex> g(d->mutex);d->drawBook.Erase(reinterpret_cast<uintptr_t>(cb));d->geometry.erase(cb);d->commands.erase(cb);}return result;}
VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(VkQueue queue,uint32_t count,const VkSubmitInfo* info,VkFence fence){
    auto d=GetDevice(queue);if(!d)return VK_ERROR_INITIALIZATION_FAILED;std::vector<cydraw::Handle> buffers;
    for(uint32_t i=0;i<count;++i)for(uint32_t j=0;j<info[i].commandBufferCount;++j)buffers.push_back(reinterpret_cast<uintptr_t>(info[i].pCommandBuffers[j]));
    auto value=GeometrySubmit(d,buffers);auto result=Next<PFN_vkQueueSubmit>(*d,"vkQueueSubmit")(queue,count,info,fence);
    if(result==VK_SUCCESS&&!buffers.empty())cydraw::Output().Submit(reinterpret_cast<uintptr_t>(d.get()),value);return result;
}
#define CY_SUBMIT2(fn) VKAPI_ATTR VkResult VKAPI_CALL fn(VkQueue queue,uint32_t count,const VkSubmitInfo2* info,VkFence fence){ \
    auto d=GetDevice(queue);if(!d)return VK_ERROR_INITIALIZATION_FAILED;std::vector<cydraw::Handle> buffers; \
    for(uint32_t i=0;i<count;++i)for(uint32_t j=0;j<info[i].commandBufferInfoCount;++j)buffers.push_back(reinterpret_cast<uintptr_t>(info[i].pCommandBufferInfos[j].commandBuffer)); \
    auto value=GeometrySubmit(d,buffers);auto result=Next<PFN_##fn>(*d,#fn)(queue,count,info,fence); \
    if(result==VK_SUCCESS&&!buffers.empty())cydraw::Output().Submit(reinterpret_cast<uintptr_t>(d.get()),value);return result;}
CY_SUBMIT2(vkQueueSubmit2)
CY_SUBMIT2(vkQueueSubmit2KHR)
#undef CY_SUBMIT2
VKAPI_ATTR VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue,const VkPresentInfoKHR* info){auto d=GetDevice(queue);if(!d)return VK_ERROR_INITIALIZATION_FAILED;auto result=Next<PFN_vkQueuePresentKHR>(*d,"vkQueuePresentKHR")(queue,info);if(info->swapchainCount&&(result==VK_SUCCESS||result==VK_SUBOPTIMAL_KHR))cydraw::Output().Present(reinterpret_cast<uintptr_t>(d.get()));return result;}
// No buffer readback: GPU-generated geometry remains explicitly incomplete.
VKAPI_ATTR void VKAPI_CALL vkCmdDrawMeshTasksEXT(VkCommandBuffer cb,uint32_t x,uint32_t y,uint32_t z){auto d=GetDevice(cb);if(!d)return;GeometryIndirect(d,cb,1);Next<PFN_vkCmdDrawMeshTasksEXT>(*d,"vkCmdDrawMeshTasksEXT")(cb,x,y,z);}
VKAPI_ATTR void VKAPI_CALL vkCmdDrawMeshTasksNV(VkCommandBuffer cb,uint32_t count,uint32_t first){auto d=GetDevice(cb);if(!d)return;GeometryIndirect(d,cb,1);Next<PFN_vkCmdDrawMeshTasksNV>(*d,"vkCmdDrawMeshTasksNV")(cb,count,first);}
VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndirectByteCountEXT(VkCommandBuffer cb,uint32_t instances,uint32_t firstInstance,VkBuffer buffer,VkDeviceSize offset,uint32_t counterOffset,uint32_t stride){auto d=GetDevice(cb);if(!d)return;GeometryIndirect(d,cb,1);Next<PFN_vkCmdDrawIndirectByteCountEXT>(*d,"vkCmdDrawIndirectByteCountEXT")(cb,instances,firstInstance,buffer,offset,counterOffset,stride);}
VKAPI_ATTR void VKAPI_CALL vkCmdDrawMultiEXT(VkCommandBuffer cb,uint32_t count,const VkMultiDrawInfoEXT* info,uint32_t instances,uint32_t firstInstance,uint32_t stride){auto d=GetDevice(cb);if(!d)return;for(uint32_t i=0;i<count;++i){auto item=reinterpret_cast<const VkMultiDrawInfoEXT*>(reinterpret_cast<const char*>(info)+size_t(i)*stride);GeometryDraw(d,cb,item->vertexCount,instances,false);}Next<PFN_vkCmdDrawMultiEXT>(*d,"vkCmdDrawMultiEXT")(cb,count,info,instances,firstInstance,stride);}
VKAPI_ATTR void VKAPI_CALL vkCmdDrawMultiIndexedEXT(VkCommandBuffer cb,uint32_t count,const VkMultiDrawIndexedInfoEXT* info,uint32_t instances,uint32_t firstInstance,uint32_t stride,const int32_t* offset){auto d=GetDevice(cb);if(!d)return;for(uint32_t i=0;i<count;++i){auto item=reinterpret_cast<const VkMultiDrawIndexedInfoEXT*>(reinterpret_cast<const char*>(info)+size_t(i)*stride);GeometryDraw(d,cb,item->indexCount,instances,true);}Next<PFN_vkCmdDrawMultiIndexedEXT>(*d,"vkCmdDrawMultiIndexedEXT")(cb,count,info,instances,firstInstance,stride,offset);}
VKAPI_ATTR void VKAPI_CALL vkCmdBindShadersEXT(VkCommandBuffer cb,uint32_t count,const VkShaderStageFlagBits* stages,const VkShaderEXT* shaders){auto d=GetDevice(cb);if(!d)return;{std::lock_guard<std::mutex> g(d->mutex);for(uint32_t i=0;i<count;++i)if(stages[i]==VK_SHADER_STAGE_VERTEX_BIT)d->geometry[cb].pipeline=VK_NULL_HANDLE;}Next<PFN_vkCmdBindShadersEXT>(*d,"vkCmdBindShadersEXT")(cb,count,stages,shaders);}
}
#define DRAW_HOOKS(M) M(vkCreateGraphicsPipelines);M(vkDestroyPipeline);M(vkCmdDraw);M(vkCmdDrawIndexed); \
    M(vkCmdDrawIndirect);M(vkCmdDrawIndexedIndirect);M(vkCmdDrawIndirectCount);M(vkCmdDrawIndirectCountKHR);M(vkCmdDrawIndirectCountAMD); \
    M(vkCmdDrawIndexedIndirectCount);M(vkCmdDrawIndexedIndirectCountKHR);M(vkCmdDrawIndexedIndirectCountAMD); \
    M(vkCmdSetPrimitiveTopology);M(vkCmdSetPrimitiveTopologyEXT);M(vkCmdSetPrimitiveRestartEnable);M(vkCmdSetPrimitiveRestartEnableEXT); \
    M(vkCmdExecuteCommands);M(vkResetCommandBuffer);M(vkQueueSubmit);M(vkQueueSubmit2);M(vkQueueSubmit2KHR);M(vkQueuePresentKHR); \
    M(vkCmdDrawMeshTasksEXT);M(vkCmdDrawMeshTasksNV);M(vkCmdBindShadersEXT);M(vkCmdDrawMultiEXT);M(vkCmdDrawMultiIndexedEXT);M(vkCmdDrawIndirectByteCountEXT); \
    M(vkCmdDrawMeshTasksIndirectEXT);M(vkCmdDrawMeshTasksIndirectNV);M(vkCmdDrawMeshTasksIndirectCountEXT);M(vkCmdDrawMeshTasksIndirectCountNV)

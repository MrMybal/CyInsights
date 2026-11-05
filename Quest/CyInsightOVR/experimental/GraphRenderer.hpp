// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
// One batched draw per eligible render pass. No descriptors or push constants
// are changed. Vertex binding 0 and the existing dynamic state are restored.
namespace cygraphs {
struct Buffer {VkBuffer handle=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;void* mapped=nullptr;VkDeviceSize capacity=0;};
struct Recording {size_t cursor=0;std::vector<Buffer> buffers;};
struct Renderer {std::map<VkCommandBuffer,Recording> recordings;std::shared_ptr<const Sample> sample;std::vector<Vertex> vertices;uint32_t width=0,height=0;uint64_t lastDraw=0;bool logged=false;};
template<class D> void Free(D& d,Buffer& b){if(b.mapped)cytiles::Proc<PFN_vkUnmapMemory>(d,"vkUnmapMemory")(d.handle,b.memory);if(b.handle)cytiles::Proc<PFN_vkDestroyBuffer>(d,"vkDestroyBuffer")(d.handle,b.handle,nullptr);if(b.memory)cytiles::Proc<PFN_vkFreeMemory>(d,"vkFreeMemory")(d.handle,b.memory,nullptr);b={};}
template<class D> void Erase(D& d,VkCommandBuffer command){auto it=d.graphRenderer.recordings.find(command);if(it==d.graphRenderer.recordings.end())return;for(auto& b:it->second.buffers)Free(d,b);d.graphRenderer.recordings.erase(it);}
template<class D> void Cleanup(D& d){for(auto& row:d.graphRenderer.recordings)for(auto& b:row.second.buffers)Free(d,b);d.graphRenderer.recordings.clear();}
template<class D> bool Allocate(D& d,Buffer& b,size_t bytes){
    if(b.capacity>=bytes)return true;Free(d,b);if(bytes>4*1024*1024)return false;
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};info.size=bytes;info.usage=VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    if(cytiles::Proc<PFN_vkCreateBuffer>(d,"vkCreateBuffer")(d.handle,&info,nullptr,&b.handle)!=VK_SUCCESS)return false;
    VkMemoryRequirements requirements{};cytiles::Proc<PFN_vkGetBufferMemoryRequirements>(d,"vkGetBufferMemoryRequirements")(d.handle,b.handle,&requirements);
    uint32_t type=~0u;for(uint32_t i=0;i<d.memoryProperties.memoryTypeCount;++i)if((requirements.memoryTypeBits&(1u<<i))&&(d.memoryProperties.memoryTypes[i].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)){type=i;break;}
    if(type==~0u){Free(d,b);return false;}VkMemoryAllocateInfo memory{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};memory.allocationSize=requirements.size;memory.memoryTypeIndex=type;
    if(cytiles::Proc<PFN_vkAllocateMemory>(d,"vkAllocateMemory")(d.handle,&memory,nullptr,&b.memory)!=VK_SUCCESS||cytiles::Proc<PFN_vkBindBufferMemory>(d,"vkBindBufferMemory")(d.handle,b.handle,b.memory,0)!=VK_SUCCESS||cytiles::Proc<PFN_vkMapMemory>(d,"vkMapMemory")(d.handle,b.memory,0,VK_WHOLE_SIZE,0,&b.mapped)!=VK_SUCCESS){Free(d,b);return false;}
    b.capacity=bytes;return true;
}
template<class D> void Draw(D& d,VkCommandBuffer command){
    auto sample=cylink::Host().Graphs();uint64_t now=cytiles::Now();if(!sample||sample->stamp>now||now-sample->stamp>=3000000000ull)return;
    std::lock_guard<std::mutex> guard(d.mutex);auto it=d.commands.find(command);if(it==d.commands.end())return;const auto& c=it->second;
    auto pi=d.passes.find(c.pass);if(pi==d.passes.end())return;auto& pass=pi->second;
    if(c.contents!=VK_SUBPASS_CONTENTS_INLINE||!c.pipeline||!c.hasViewport||!c.hasScissor||!c.vertex0||c.subpass+1!=pass.color.size()||!pass.color[c.subpass]||c.area.extent.width<512||c.area.extent.height<512)return;
    // This selects a render surface candidate, not an XR eye/frame identity.
    auto& r=d.graphRenderer;if(!r.lastDraw||now-r.lastDraw>1000000000ull||uint64_t(c.area.extent.width)*c.area.extent.height>uint64_t(r.width)*r.height){r.width=c.area.extent.width;r.height=c.area.extent.height;r.sample.reset();}
    if(c.area.extent.width!=r.width||c.area.extent.height!=r.height)return;
    if(r.sample!=sample){r.vertices=Geometry(*sample,float(r.width),float(r.height));r.sample=sample;}if(r.vertices.empty())return;
    auto& recording=r.recordings[command];if(recording.cursor>=16)return;if(recording.cursor==recording.buffers.size())recording.buffers.emplace_back();auto& buffer=recording.buffers[recording.cursor++];
    // A buffer belongs to this command-buffer recording and is reused only after
    // the application legally begins recording it again (therefore not pending).
    if(!Allocate(d,buffer,r.vertices.size()*sizeof(Vertex)))return;std::memcpy(buffer.mapped,r.vertices.data(),r.vertices.size()*sizeof(Vertex));
    if(!pass.graphs[c.subpass])pass.graphs[c.subpass]=cytiles::Pipeline(d,c.pass,c.subpass,pass.samples[c.subpass],false,true);if(!pass.graphs[c.subpass])return;
    auto bind=cytiles::Proc<PFN_vkCmdBindPipeline>(d,"vkCmdBindPipeline");auto vertex=cytiles::Proc<PFN_vkCmdBindVertexBuffers>(d,"vkCmdBindVertexBuffers");VkDeviceSize offset=0;
    VkViewport viewport{float(c.area.offset.x),float(c.area.offset.y),float(r.width),float(r.height),0,1};
    bind(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pass.graphs[c.subpass]);vertex(command,0,1,&buffer.handle,&offset);cytiles::Proc<PFN_vkCmdSetViewport>(d,"vkCmdSetViewport")(command,0,1,&viewport);cytiles::Proc<PFN_vkCmdSetScissor>(d,"vkCmdSetScissor")(command,0,1,&c.area);cytiles::Proc<PFN_vkCmdDraw>(d,"vkCmdDraw")(command,uint32_t(r.vertices.size()),1,0,0);
    if(c.vertexBinding2){auto restore=cytiles::Proc<PFN_vkCmdBindVertexBuffers2>(d,"vkCmdBindVertexBuffers2");if(!restore)restore=cytiles::Proc<PFN_vkCmdBindVertexBuffers2>(d,"vkCmdBindVertexBuffers2EXT");restore(command,0,1,&c.vertex0,&c.vertexOffset,&c.vertexSize,nullptr);}else vertex(command,0,1,&c.vertex0,&c.vertexOffset);
    bind(command,VK_PIPELINE_BIND_POINT_GRAPHICS,c.pipeline);cytiles::Proc<PFN_vkCmdSetViewport>(d,"vkCmdSetViewport")(command,0,1,&c.viewport);cytiles::Proc<PFN_vkCmdSetScissor>(d,"vkCmdSetScissor")(command,0,1,&c.scissor);
    r.lastDraw=now;
    cylink::Host().Recorded(now,r.width,r.height,uint32_t(sample->curves.size()));
    if(!r.logged){r.logged=true;__android_log_print(ANDROID_LOG_INFO,"CyOVRGraphs","Measured curve HUD recorded: graphs=%zu vertices=%zu surface=%ux%u",sample->curves.size(),r.vertices.size(),r.width,r.height);}
}
}

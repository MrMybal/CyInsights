// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "LiveTiles.hpp"
#include "TileShaders.hpp"
namespace cytiles {
struct Pass {std::vector<VkSampleCountFlagBits> samples;std::vector<bool> color;std::vector<VkPipeline> pipelines,labels;bool logged=false;};
struct Command {VkRenderPass pass=VK_NULL_HANDLE;VkRect2D area{};uint32_t subpass=0;VkSubpassContents contents=VK_SUBPASS_CONTENTS_INLINE;VkPipeline pipeline=VK_NULL_HANDLE;VkViewport viewport{};VkRect2D scissor{};std::array<float,4> blend{};bool hasViewport=false,hasScissor=false;};
template<class F,class D> F Proc(D& d,const char* name){return reinterpret_cast<F>(d.gpa(d.handle,name));}
template<class D> bool Shader(D& d,const uint32_t* code,size_t size,VkShaderModule& module){if(module)return true;VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};info.codeSize=size;info.pCode=code;return Proc<PFN_vkCreateShaderModule>(d,"vkCreateShaderModule")(d.handle,&info,nullptr,&module)==VK_SUCCESS;}
template<class D> VkPipeline Pipeline(D& d,VkRenderPass rp,uint32_t subpass,VkSampleCountFlagBits samples,bool label){
    if(!Shader(d,Vert,sizeof(Vert),d.vert)||!Shader(d,Frag,sizeof(Frag),d.frag)||!Shader(d,Label,sizeof(Label),d.label))return VK_NULL_HANDLE;
    if(!d.layout){VkPipelineLayoutCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};if(Proc<PFN_vkCreatePipelineLayout>(d,"vkCreatePipelineLayout")(d.handle,&info,nullptr,&d.layout)!=VK_SUCCESS)return VK_NULL_HANDLE;}
    VkPipelineShaderStageCreateInfo shaders[2]{};for(auto& s:shaders){s.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;s.pName="main";}shaders[0].stage=VK_SHADER_STAGE_VERTEX_BIT;shaders[0].module=d.vert;shaders[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;shaders[1].module=label?d.label:d.frag;
    VkPipelineVertexInputStateCreateInfo vertex{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};viewport.viewportCount=viewport.scissorCount=1;
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};raster.polygonMode=VK_POLYGON_MODE_FILL;raster.cullMode=VK_CULL_MODE_NONE;raster.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;raster.lineWidth=1;
    VkPipelineMultisampleStateCreateInfo multi{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};multi.rasterizationSamples=samples;
    VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};depth.depthCompareOp=VK_COMPARE_OP_ALWAYS;
    VkPipelineColorBlendAttachmentState attachment{};attachment.blendEnable=VK_TRUE;attachment.srcColorBlendFactor=VK_BLEND_FACTOR_CONSTANT_COLOR;attachment.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;attachment.colorBlendOp=VK_BLEND_OP_ADD;attachment.srcAlphaBlendFactor=VK_BLEND_FACTOR_ZERO;attachment.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE;attachment.alphaBlendOp=VK_BLEND_OP_ADD;attachment.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT;
    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};blend.attachmentCount=1;blend.pAttachments=&attachment;
    VkDynamicState dynamicValues[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_BLEND_CONSTANTS};VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};dynamic.dynamicStateCount=3;dynamic.pDynamicStates=dynamicValues;
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};info.stageCount=2;info.pStages=shaders;info.pVertexInputState=&vertex;info.pInputAssemblyState=&assembly;info.pViewportState=&viewport;info.pRasterizationState=&raster;info.pMultisampleState=&multi;info.pDepthStencilState=&depth;info.pColorBlendState=&blend;info.pDynamicState=&dynamic;info.layout=d.layout;info.renderPass=rp;info.subpass=subpass;
    VkPipeline out=VK_NULL_HANDLE;VkResult result=Proc<PFN_vkCreateGraphicsPipelines>(d,"vkCreateGraphicsPipelines")(d.handle,VK_NULL_HANDLE,1,&info,nullptr,&out);
    if(result!=VK_SUCCESS)__android_log_print(ANDROID_LOG_ERROR,"CyOVRVulkan","HUD pipeline failed result=%d",int(result));return out;
}
template<class D> void DestroyPass(D& d,Pass& p){auto destroy=Proc<PFN_vkDestroyPipeline>(d,"vkDestroyPipeline");for(auto pipeline:p.pipelines)if(pipeline)destroy(d.handle,pipeline,nullptr);for(auto pipeline:p.labels)if(pipeline)destroy(d.handle,pipeline,nullptr);}
template<class D> void Cleanup(D& d){for(auto& p:d.passes)DestroyPass(d,p.second);d.passes.clear();auto destroy=Proc<PFN_vkDestroyShaderModule>(d,"vkDestroyShaderModule");for(auto m:{d.vert,d.frag,d.label})if(m)destroy(d.handle,m,nullptr);if(d.layout)Proc<PFN_vkDestroyPipelineLayout>(d,"vkDestroyPipelineLayout")(d.handle,d.layout,nullptr);}
template<class D> void Rect(D& d,VkCommandBuffer command,const Command& state,float x,float y,float w,float h,float r,float g,float b,float opacity){
    if(w<1||h<1)return;VkViewport viewport{x,y,w,h,0,1};
    int32_t left=std::max(int32_t(std::ceil(x)),state.area.offset.x),top=std::max(int32_t(std::ceil(y)),state.area.offset.y);
    int32_t right=std::min(int32_t(std::floor(x+w)),state.area.offset.x+int32_t(state.area.extent.width)),bottom=std::min(int32_t(std::floor(y+h)),state.area.offset.y+int32_t(state.area.extent.height));if(right<=left||bottom<=top)return;
    VkRect2D scissor{{left,top},{uint32_t(right-left),uint32_t(bottom-top)}};float blend[]={r*opacity,g*opacity,b*opacity,opacity};
    Proc<PFN_vkCmdSetViewport>(d,"vkCmdSetViewport")(command,0,1,&viewport);Proc<PFN_vkCmdSetScissor>(d,"vkCmdSetScissor")(command,0,1,&scissor);Proc<PFN_vkCmdSetBlendConstants>(d,"vkCmdSetBlendConstants")(command,blend);Proc<PFN_vkCmdDraw>(d,"vkCmdDraw")(command,3,1,0,0);
}
template<class D> void Draw(D& d,VkCommandBuffer command){
    auto sample=d.reader.Latest();if(!sample)return;std::lock_guard<std::mutex> lock(d.mutex);
    auto ci=d.commands.find(command);if(ci==d.commands.end())return;const Command& c=ci->second;auto pi=d.passes.find(c.pass);if(pi==d.passes.end())return;auto& pass=pi->second;
    // Only inline color subpasses with fully tracked dynamic state are supported.
    // Dimensions choose a display candidate, never establish per-eye/frame identity.
    if(c.area.extent.width!=sample->width||c.area.extent.height!=sample->height||c.contents!=VK_SUBPASS_CONTENTS_INLINE||!c.pipeline||!c.hasViewport||!c.hasScissor||c.subpass>=pass.color.size()||!pass.color[c.subpass])return;
    if(!pass.pipelines[c.subpass])pass.pipelines[c.subpass]=Pipeline(d,c.pass,c.subpass,pass.samples[c.subpass],false);
    if(!pass.labels[c.subpass])pass.labels[c.subpass]=Pipeline(d,c.pass,c.subpass,pass.samples[c.subpass],true);
    auto pipeline=pass.pipelines[c.subpass],label=pass.labels[c.subpass];if(!pipeline||!label)return;
    const uint32_t position=sample->position&255u;const bool fullscreen=(sample->position&256u)!=0;
    float w=fullscreen?std::min(float(sample->width)-48,(float(sample->height)-72)*sample->width/sample->height):std::min(420.f,sample->width*.24f),h=w*sample->height/sample->width;float margin=24,top=24;
    bool left=position==0||position==2||position==5,right=position==1||position==3||position==7;
    bool upper=position==0||position==1||position==4,lower=position==2||position==3||position==8;
    float safeX=fullscreen?margin:sample->width*.15f,safeY=fullscreen?margin:sample->height*.15f;
    float x=c.area.offset.x+(left?safeX:right?sample->width-w-safeX:(sample->width-w)*.5f);
    float y=c.area.offset.y+(upper?safeY+top:lower?sample->height-h-safeY:(sample->height-h)*.5f);
    auto bind=Proc<PFN_vkCmdBindPipeline>(d,"vkCmdBindPipeline");bind(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
    Rect(d,command,c,x-4,y-top,w+8,h+top+4,.02f,.03f,.05f,sample->opacity*.2f);
    for(const auto& t:sample->tiles){float n=std::clamp(t.cost/sample->ceiling,0.f,1.f);float r=t.cost<0?.35f:std::min(1.f,n*2),g=t.cost<0?.35f:std::min(1.f,(1-n)*2),b=t.cost<0?.35f:.12f;
        Rect(d,command,c,x+t.x/sample->width*w,y+t.y/sample->height*h,std::max(1.f,t.width/sample->width*w-1),std::max(1.f,t.height/sample->height*h-1),r,g,b,sample->opacity);}
    bind(command,VK_PIPELINE_BIND_POINT_GRAPHICS,label);Rect(d,command,c,x,y-top+4,w,14,.8f,.95f,1,std::min(1.f,sample->opacity*2));
    bind(command,VK_PIPELINE_BIND_POINT_GRAPHICS,c.pipeline);Proc<PFN_vkCmdSetViewport>(d,"vkCmdSetViewport")(command,0,1,&c.viewport);Proc<PFN_vkCmdSetScissor>(d,"vkCmdSetScissor")(command,0,1,&c.scissor);Proc<PFN_vkCmdSetBlendConstants>(d,"vkCmdSetBlendConstants")(command,c.blend.data());
    if(!pass.logged){pass.logged=true;__android_log_print(ANDROID_LOG_INFO,"CyOVRVulkan","HUD commands recorded rp=%p subpass=%u tiles=%zu; visibility requires confirmation",c.pass,c.subpass,sample->tiles.size());}
}
}

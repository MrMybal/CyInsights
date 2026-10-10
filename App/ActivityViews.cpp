// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "Application.hpp"
#include "UiMetrics.hpp"
#include <imgui_internal.h>
#include <fstream>
#include <sstream>
#include <algorithm>
namespace cyinsights {
void Application::StartActivities(){
 if(activity_reader_.Busy()||trace_.empty()||native_.Busy())return;
 activity_auto_started_=true;
 try{
  auto binaries=fs::u8path(exe_).parent_path();std::ifstream version(binaries.parent_path().parent_path()/"Build/Build.version");
  std::string text((std::istreambuf_iterator<char>(version)),{});cygi::Json metadata;
  if(!cygi::Json::Parse(text,metadata))throw std::runtime_error("Select an installed Unreal Insights analyzer in Session first");
  int minor=static_cast<int>(metadata["MinorVersion"].AsNumber());
  if(metadata["MajorVersion"].AsNumber()!=5||minor<5||minor>8)throw std::runtime_error("Activity reader requires an installed UE 5.5-5.8 analyzer; older source traces remain readable when supported by that analyzer");
  auto helper=FindNativeReader("Activities",fs::u8path(exe_));
  if(activity_reader_.Start(helper,binaries,trace_,cache_/"Activities",activity_message_))activity_message_="Decoding network, file, loading and memory providers...";
 }catch(const std::exception& e){activity_message_=e.what();}
}
void Application::ActivityTick(){
 if(activity_reader_.Poll(activity_store_,activity_message_)&&activity_store_){
  activity_result_={};activity_ready_.clear();activity_request_.clear();activity_details_ready_.clear();activity_details_observed_.clear();activity_pending_=false;activity_pinned_.assign(activity_store_->lanes.size(),0);++activity_revision_;
  for(size_t i=0;i<activity_pinned_.size();++i){auto& lane=activity_store_->lanes[i];activity_pinned_[i]=lane.kind!="memory"||lane.label=="Memory / total allocated max";}
  if(!activity_store_->lanes.empty()){activity_lane_=0;
   if(!native_.store&&native_.events.empty()){timing_model_.start=activity_store_->start;timing_model_.end=std::max(activity_store_->end,activity_store_->start+.000001);FitCapture();}
   else {timing_model_.start=std::min(timing_model_.start,activity_store_->start);timing_model_.end=std::max(timing_model_.end,activity_store_->end);}
  }
  bool memoryPinned=false;for(size_t i=0;i<activity_pinned_.size();++i)memoryPinned|=activity_pinned_[i]&&activity_store_->lanes[i].kind=="memory";
  if(!memoryPinned)for(size_t i=0;i<activity_pinned_.size();++i)if(activity_store_->lanes[i].kind=="memory"){activity_pinned_[i]=1;break;}
 }
 if(!activity_store_||activity_store_->lanes.empty()){activity_pending_=false;return;}
 double a=range_start_==range_end_?view_start_:std::min(range_start_,range_end_),b=range_start_==range_end_?view_end_:std::max(range_start_,range_end_);
 int pixels=std::clamp(static_cast<int>(activity_width_),1,16384);
 std::ostringstream key,details;key.precision(17);details.precision(17);
 key<<view_start_<<'/'<<view_end_<<'/'<<pixels<<'/'<<activity_revision_;auto current=key.str();
 details<<a<<'/'<<b<<'/'<<activity_lane_<<'/'<<activity_page_<<'/'<<activity_search_;auto currentDetails=details.str();
 double now=ImGui::GetTime();if(activity_details_observed_!=currentDetails){activity_details_observed_=currentDetails;activity_details_changed_at_=now;}
 // Finish a bounded read instead of cancelling on every pointer movement.
 // Plot and detail keys are independent: paging/search never rereads the plot.
 if(activity_query_.valid()&&activity_query_.wait_for(std::chrono::seconds(0))==std::future_status::ready){
  try{auto result=activity_query_.get();if(result.has_visible){activity_result_.visible=std::move(result.visible);activity_ready_=activity_request_;}if(result.has_details&&currentDetails==activity_details_request_){activity_result_.details=std::move(result.details);activity_details_ready_=currentDetails;}}
  catch(const std::exception& e){if(!activity_cancel_.load()){activity_message_=e.what();activity_ready_=current;activity_details_ready_=currentDetails;}}
 }
 bool needVisible=activity_ready_!=current,needDetails=show_activities_&&activity_details_ready_!=currentDetails;
 activity_pending_=needVisible||needDetails;if(activity_query_.valid()||!activity_pending_)return;
 bool queryDetails=needDetails&&drag_surface_==DragSurface::None&&!ImGui::IsMouseDown(0)&&now-activity_details_changed_at_>=.075;
 bool queryVisible=needVisible&&(activity_ready_.empty()||now-activity_requested_at_>=1./30.);
 if(!queryVisible&&!queryDetails)return;
 activity_plot_queries_+=queryVisible;activity_detail_queries_+=queryDetails;
 activity_cancel_=false;activity_requested_at_=now;activity_request_=queryVisible?current:std::string();activity_details_request_=queryDetails?currentDetails:std::string();auto store=activity_store_;auto selected=static_cast<uint32_t>(activity_lane_);auto page=activity_page_;std::string search=activity_search_;std::vector<uint32_t> lanes;
 for(size_t i=0;i<activity_pinned_.size();++i)if(activity_pinned_[i])lanes.push_back(static_cast<uint32_t>(i));
 auto start=view_start_,end=view_end_;
 activity_query_=std::async(std::launch::async,[this,store,lanes,selected,page,search,start,end,a,b,pixels,queryVisible,queryDetails]{
  ActivityResult r;r.has_visible=queryVisible;r.has_details=queryDetails;if(queryVisible)r.visible=store->Query(start,end,lanes,pixels,{},0,&activity_cancel_);if(queryDetails)r.details=store->Query(a,b,{selected},1,search,page,&activity_cancel_);return r;
 });
}
void Application::DockActivities(){
 if(!activity_dock_pending_)return;
 auto* timing=ImGui::FindWindowByName("Timing");auto* activity=ImGui::FindWindowByName("Activities");
 if(!timing||!timing->DockId)return;
 activity_dock_pending_=false;
 if(activity&&activity->DockId&&activity->DockId!=timing->DockId)return;
 auto* node=ImGui::DockBuilderGetNode(timing->DockId);if(!node||!node->IsLeafNode())return;
 auto* root=node;while(root->ParentNode)root=root->ParentNode;auto id=root->ID;ImGuiID bottom=0,top=0;
 ImGui::DockBuilderSplitNode(node->ID,ImGuiDir_Down,.30f,&bottom,&top);
 ImGui::DockBuilderDockWindow("Activities",bottom);ImGui::DockBuilderFinish(id);
}
void Application::ActivitiesView(){
 // A requested analysis also starts when its dock tab was previously hidden.
 if(!activity_store_&&!activity_auto_started_&&!trace_.empty()&&!native_.Busy())StartActivities();
 bool visible=ImGui::Begin("Activities",&show_activities_);
 if(focus_activities_){auto* window=ImGui::GetCurrentWindow();if(window->DockNode&&window->DockNode->TabBar){window->DockNode->SelectedTabId=window->TabId;window->DockNode->TabBar->NextSelectedTabId=window->TabId;}ImGui::SetWindowFocus();focus_activities_=false;}
 if(!visible){ImGui::End();return;}
 ImGui::TextWrapped("Network / storage / asset loading / memory on the native trace clock.");
 if(activity_store_&&!activity_message_.empty()&&activity_message_.find("Native activities ready:")!=0)ImGui::TextWrapped("%s",activity_message_.c_str());
 if(!activity_store_){
  ImGui::BeginDisabled(trace_.empty()||native_.Busy()||activity_reader_.Busy());if(ImGui::Button("Load native activities"))StartActivities();ImGui::EndDisabled();
  if(activity_reader_.Busy()){ImGui::Text("%s",activity_reader_.Phase()==1?"Analyzing installed Unreal providers...":"Building activity disk index...");ImGui::ProgressBar(-static_cast<float>(ImGui::GetTime()),ImVec2(-1,0));if(ImGui::SmallButton("Cancel activity loading"))activity_reader_.Cancel();}
  if(!activity_message_.empty())ImGui::TextWrapped("%s",activity_message_.c_str());
  ImGui::TextWrapped("Record net with -NetTrace=1; file and loadtime for storage/loading. Allocation tracing must be enabled at launch with -trace=memory. LLM tag samples require LLM tracing. Missing events cannot be recovered from an existing trace.");
  ImGui::End();return;
 }
 if(activity_store_->lanes.empty()){ImGui::TextWrapped("No network, storage, loading or memory observations recorded. Enable their capture channels before recording.");ImGui::End();return;}
 const char* kinds[]={"network","storage","loading","memory"};for(auto kind:kinds){auto n=std::count_if(activity_store_->lanes.begin(),activity_store_->lanes.end(),[&](auto& l){return l.kind==kind;});ImGui::Text("%s: %s (%zu lanes)",kind,n?"recorded":"unavailable in this capture",static_cast<size_t>(n));}
 if(ImGui::BeginCombo("Track",activity_store_->lanes[activity_lane_].label.c_str())){ImGuiListClipper clip;clip.Begin(static_cast<int>(activity_store_->lanes.size()));while(clip.Step())for(int i=clip.DisplayStart;i<clip.DisplayEnd;++i)if(ImGui::Selectable(activity_store_->lanes[i].label.c_str(),i==activity_lane_)){activity_lane_=i;activity_page_=0;}ImGui::EndCombo();}
 bool pin=activity_pinned_[activity_lane_]!=0;if(ImGui::Checkbox("Show track in Timing",&pin)){activity_pinned_[activity_lane_]=pin;++activity_revision_;}
 if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)&&ImGui::GetIO().KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_F))ImGui::SetKeyboardFocusHere();
 if(ImGui::InputTextWithHint("##activity_search","Search event / file / packet content (Ctrl+F)",activity_search_,sizeof(activity_search_)))activity_page_=0;
 ImGui::TextDisabled(range_start_==range_end_?"Details follow visible interval":"Details follow selected timeline interval (Shift + drag ruler)");
 if(activity_query_.valid()||activity_pending_)ImGui::TextDisabled("Updating interval... previous result retains its displayed time bounds");
 if(activity_result_.details.lanes.empty()){ImGui::End();return;}
 const auto& window=activity_result_.details;const auto& data=window.lanes.front();if(data.lane!=static_cast<uint32_t>(activity_lane_)){ImGui::End();return;}
 const auto& lane=activity_store_->lanes[data.lane];
 ImGui::Text("%.6f - %.6f s / %llu observations",window.start,window.end,data.matched);
 if(!data.matched)ImGui::TextDisabled("No observations in this interval; values unavailable");
 else if(lane.kind=="memory"&&data.signed_values)ImGui::Text("Min / max: %lld / %lld %s (native sample values)",data.signed_minimum,data.signed_maximum,lane.unit.c_str());
 else if(lane.kind=="memory")ImGui::Text("Min / max: %llu / %llu %s (native sample values)",data.minimum,data.maximum,lane.unit.c_str());
 else ImGui::Text("%llu %s at operation/packet starts / %.3f ms clipped inclusive duration",data.value_sum,lane.unit.c_str(),data.inclusive_seconds*1000);
 if(lane.kind=="network")ImGui::TextWrapped("%llu packet-content observations; their bit sizes are separate from packet bytes. Time is send/receive observation; transit duration unavailable.",data.content);
 if(lane.kind=="storage")ImGui::TextDisabled("Unreal file operations; OS disk busy time unavailable. Concurrent operation durations can overlap.");
 if(lane.kind=="loading")ImGui::TextDisabled("Asset loading CPU scopes; serialized bytes are separate from physical disk traffic.");
 if(lane.kind=="memory")ImGui::TextDisabled("Samples/count buckets describe memory state, not CPU execution duration or individual allocation callstacks.");
 if(lane.kind=="memory")ImGui::TextDisabled(lane.label.find("Memory / total allocated")==0||lane.unit=="events"?"Derived summary of native allocation events":"Measured native LLM sample values");
 ImGui::BeginDisabled(activity_page_==0);if(ImGui::SmallButton("Previous observations"))--activity_page_;ImGui::EndDisabled();ImGui::SameLine();
 ImGui::BeginDisabled((activity_page_+1)*128>=data.matched);if(ImGui::SmallButton("Next observations"))++activity_page_;ImGui::EndDisabled();
 if(ImGui::BeginTable("activity_observations",6,ImGuiTableFlags_Resizable|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY)){
  for(auto title:{"Name","Start s","Duration ms","Value","Identity / offset","Thread"})ImGui::TableSetupColumn(title);ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
  for(size_t i=0;i<data.rows.size();++i){auto& e=data.rows[i];ImGui::PushID(static_cast<int>(i));ImGui::TableNextRow();ImGui::TableNextColumn();auto label=activity_store_->names[e.name]+((e.flags&ActivityIncomplete)?" [incomplete]":(e.flags&ActivityFailed)?" [failed]":"");
   if(ImGui::Selectable(label.c_str(),false,ImGuiSelectableFlags_SpanAllColumns)){auto b=std::max(e.end,e.start+.00001);range_start_=e.start;range_end_=b;auto pad=std::max(.001,(b-e.start)*.1);view_start_=e.start-pad;view_end_=b+pad;activity_page_=0;}
   ImGui::TableNextColumn();ImGui::Text("%.9f",e.start);ImGui::TableNextColumn();if(e.flags&(ActivityPoint|ActivityGauge|ActivityBucketCount|ActivityIncomplete))ImGui::TextDisabled("Unavailable");else ImGui::Text("%.6f",(e.end-e.start)*1000);
   ImGui::TableNextColumn();if(e.flags&ActivitySignedGauge)ImGui::Text("%lld %s",static_cast<int64_t>(e.value),lane.unit.c_str());else ImGui::Text("%llu %s",e.value,e.flags&ActivityContentBits?"bits":lane.unit.c_str());ImGui::TableNextColumn();ImGui::Text("%llu",e.identity);ImGui::TableNextColumn();if(e.thread==UINT32_MAX)ImGui::TextDisabled("Unavailable");else ImGui::Text("%u",e.thread);ImGui::PopID();
  }ImGui::EndTable();
 }ImGui::End();
}
void Application::ActivityTracks(float width){
 activity_width_=width;if(!activity_store_)return;auto scale=UiScale();const float header=18*scale,height=76*scale;auto* draw=ImGui::GetWindowDrawList();auto clip=ImGui::GetCurrentWindow()->ClipRect;
 for(size_t lane=0;lane<activity_pinned_.size();++lane){if(!activity_pinned_[lane])continue;auto pos=ImGui::GetCursorScreenPos();ImGui::PushID(static_cast<int>(lane));ImGui::InvisibleButton("activity_lane",ImVec2(width,height));bool hover=ImGui::IsItemHovered();
  if(hover&&ImGui::IsMouseClicked(0)&&ImGui::GetIO().MousePos.y>=pos.y+header)BeginTimingDrag(DragSurface::Tracks,0,width,false);
  if(hover&&ImGui::IsMouseClicked(0)&&ImGui::GetIO().MousePos.y<pos.y+header){activity_lane_=static_cast<int>(lane);activity_page_=0;FocusActivities();}
  ImGui::PopID();if(pos.y+height<clip.Min.y||pos.y>clip.Max.y)continue;
  auto& meta=activity_store_->lanes[lane];auto colour=meta.kind=="network"?IM_COL32(94,201,222,255):meta.kind=="memory"?IM_COL32(169,131,216,255):IM_COL32(225,178,101,255);
  draw->AddRectFilled(pos,ImVec2(pos.x+width,pos.y+height),IM_COL32(67,67,67,255));draw->AddRectFilled(pos,ImVec2(pos.x+width,pos.y+header),IM_COL32(58,58,58,255));
  draw->PushClipRect(pos,ImVec2(pos.x+width,pos.y+height),true);draw->AddText(ImVec2(pos.x+4*scale,pos.y+2*scale),colour,meta.label.c_str());
  auto& window=activity_result_.visible;auto found=std::find_if(window.lanes.begin(),window.lanes.end(),[&](auto& l){return l.lane==lane;});
  if(window.end>window.start&&window.start<view_end_&&window.end>view_start_&&found!=window.lanes.end()&&!found->bins.empty()){
   double peak=1,low=0;for(auto& bin:found->bins){peak=std::max(peak,meta.kind=="memory"?bin.maximum:double(bin.count));if(meta.kind=="memory")low=std::min(low,bin.minimum);}
   double span=std::max(.000001,view_end_-view_start_),column=(window.end-window.start)/found->bins.size();
   float base=pos.y+height-4*scale;for(size_t x=0;x<found->bins.size();++x){auto& bin=found->bins[x];if(!bin.count)continue;
    double t=window.start+column*x;if(t+column<=view_start_||t>=view_end_)continue;
    float px=pos.x+width*float((t-view_start_)/span),right=pos.x+width*float((t+column-view_start_)/span),y=base-float((meta.kind=="memory"?(bin.maximum-low)/(peak-low):double(bin.count)/peak))*(height-header-12*scale);
    draw->AddRectFilled(ImVec2(px,y),ImVec2(std::max(px+1,right),base),colour);
   }
   if(window.start!=view_start_||window.end!=view_end_)draw->AddText(ImVec2(pos.x+5*scale,pos.y+header+2*scale),IM_COL32(220,220,220,255),"Updating... last completed density reprojected");
   double hoveredTime=view_start_+(ImGui::GetIO().MousePos.x-pos.x)/width*span;
   if(hover&&hoveredTime>=window.start&&hoveredTime<window.end){auto x=static_cast<size_t>(std::clamp((hoveredTime-window.start)/column,0.,double(found->bins.size()-1)));auto& bin=found->bins[x];
    if(meta.kind=="memory")ImGui::SetTooltip("%s\n%llu native samples in column\nMin / max: %.0f / %.0f %s\nSample intervals are not execution durations.",meta.label.c_str(),bin.count,bin.minimum,bin.maximum,meta.unit.c_str());
    else ImGui::SetTooltip("%s\n%llu observations intersect this time column\nDerived density display of measured events; open Activities for exact observations.",meta.label.c_str(),bin.count);
   }
  }else draw->AddText(ImVec2(pos.x+5*scale,pos.y+header+6*scale),IM_COL32(180,180,180,255),"Updating activity interval...");
  if(range_start_!=range_end_){double span=std::max(.000001,view_end_-view_start_);float a=pos.x+width*float((std::min(range_start_,range_end_)-view_start_)/span),b=pos.x+width*float((std::max(range_start_,range_end_)-view_start_)/span);draw->AddRectFilled(ImVec2(a,pos.y+header),ImVec2(b,pos.y+height),IM_COL32(88,143,195,35));}
  draw->PopClipRect();
 }
}
}

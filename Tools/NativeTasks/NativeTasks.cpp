// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
// Optional adapter for licensed installed Unreal TraceAnalysis binaries.
// No engine source is compiled or Epic DLL redistributed.
#include <Windows.h>
#include <strsafe.h>
#include "Trace/Analysis.h"
#include "Trace/Analyzer.h"
#include "Trace/DataStream.h"
#include <cstdio>
#include <cstring>
#include <string>
class Input final:public UE::Trace::IInDataStream {
public:
 FILE* file=nullptr;
 explicit Input(const wchar_t* path){file=_wfopen(path,L"rb");}
 ~Input(){if(file)fclose(file);}
 int32 Read(void* data,uint32 size) override{return static_cast<int32>(fread(data,1,size,file));}
};
static std::string Utf8(FWideStringView value) {
 int count=WideCharToMultiByte(CP_UTF8,0,value.GetData(),value.Len(),nullptr,0,nullptr,nullptr);
 std::string result(count,'\0');if(count)WideCharToMultiByte(CP_UTF8,0,value.GetData(),value.Len(),result.data(),count,nullptr,nullptr);return result;
}
static void String(FILE* out,const std::string& value){fputc('"',out);for(char c:value){if(c=='"')fputc('"',out);if(c=='\r'||c=='\n')fputc(' ',out);else fputc(c,out);}fputc('"',out);}
class Tasks final:public UE::Trace::IAnalyzer {
public:
 FILE* out;unsigned long long count=0;bool unsupported=false;
 explicit Tasks(FILE* output):out(output){}
 void OnAnalysisBegin(const FOnAnalysisContext& context) override {context.InterfaceBuilder.RouteLoggerEvents(1,"TaskTrace");}
 void OnThreadInfo(const FThreadInfo& info) override {
  fprintf(out,"Thread,0,%u,%u,0,",info.GetId(),info.GetId());String(out,info.GetName()?info.GetName():"");fprintf(out,",0,0,0\n");
 }
 bool OnEvent(uint16,EStyle,const FOnEventContext& context) override {
  const auto& data=context.EventData;const char* event=data.GetTypeInfo().GetName();
  if(!strcmp(event,"Init")){auto version=data.GetValue<uint32>("Version");unsupported|=version!=0;fprintf(out,"Init,0,%u,0,0,,0,0,0\n",version);return true;}
  // Wait-for lists and legacy NestedAdded have a different schema. Their raw
  // source remains intact; the UI does not invent those relations.
  if(strcmp(event,"Created")&&strcmp(event,"Launched")&&strcmp(event,"Scheduled")&&strcmp(event,"Started")&&strcmp(event,"Finished")&&strcmp(event,"Completed")&&strcmp(event,"Destroyed")&&strcmp(event,"SubsequentAdded"))return true;
  auto field=[&](const char* name){const auto& type=data.GetTypeInfo();for(uint32 i=0;i<type.GetFieldCount();++i){auto* info=type.GetFieldInfo(i);if(!strcmp(info->GetName(),name))return info->GetSize()==8&&!info->IsArray();}return false;};
  if(!field("TaskId")||!field("Timestamp")||(!strcmp(event,"SubsequentAdded")&&!field("SubsequentId"))){unsupported=true;return true;}
  ++count;auto id=data.GetValue<uint64>("TaskId");double time=context.EventTime.AsSeconds(data.GetValue<uint64>("Timestamp"));
  fprintf(out,"%s,%.17g,%llu,%u,%llu,",event,time,id,context.ThreadInfo.GetId(),data.GetValue<uint64>("SubsequentId"));
  FWideStringView wide;FAnsiStringView ansi;
  if(data.GetString("DebugName",wide))String(out,Utf8(wide));else if(data.GetString("DebugName",ansi))String(out,std::string(ansi.GetData(),ansi.Len()));else String(out,"");
  fprintf(out,",%u,%d,%llu\n",data.GetValue<bool>("Tracked")?1:0,data.GetValue<int32>("ThreadToExecuteOn"),data.GetValue<uint64>("TaskSize"));return true;
 }
};
int wmain(int count,wchar_t** args) {
 if(count!=3){fprintf(stderr,"Usage: CyNativeTasks trace.utrace tasks.csv\n");return 2;}
 Input input(args[1]);if(!input.file)return 3;
 FILE* out=_wfopen(args[2],L"wb");if(!out)return 4;
 fprintf(out,"Event,Time,TaskId,ThreadId,RelatedId,Name,Tracked,TargetThread,Size\n");
 Tasks tasks(out);bool failed=false;UE::Trace::FAnalysisContext analysis;analysis.AddAnalyzer(tasks);
 analysis.SetMessageDelegate(UE::Trace::FMessageDelegate::CreateLambda([&](UE::Trace::EAnalysisMessageSeverity level,FStringView){if(level==UE::Trace::EAnalysisMessageSeverity::Error)failed=true;}));
 auto processor=analysis.Process(input);processor.Wait();failed|=tasks.unsupported||ferror(input.file)!=0||ferror(out)!=0;
 if(!failed)fprintf(out,"End,0,%llu,0,0,,0,0,0\n",tasks.count);
 failed|=fclose(out)!=0;fprintf(stdout,"Task lifecycle events: %llu\n",tasks.count);return failed?5:0;
}

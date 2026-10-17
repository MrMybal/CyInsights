// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "AiResources.hpp"
#include <iostream>
#include <set>
int main(){
    using namespace cyinsights;int checks=0,failed=0;auto check=[&](bool ok,const char* text){++checks;if(!ok){++failed;std::cerr<<text<<'\n';}};
    const auto& catalog=EmbeddedAiCatalog();const auto& method=EmbeddedAiMethod();
    check(catalog["operations"].Size()==39,"All standalone operation contracts embedded");
    check(method["id"].AsString()=="cyinsights.trace-analysis"&&method["workflow"].Size()==9,"Versioned method and workflows embedded");
    std::set<std::string> operations;for(const auto& op:catalog["operations"].Items())operations.insert(op["id"].AsString());
    for(const auto& step:method["workflow"].Items())for(const auto& op:step["tools"].Items())check(operations.count(op.AsString())!=0,"Every guided action has a native operation");
    check(AiUiText("Guided trace analysis",true)!="Guided trace analysis","French interface strings embedded");
    for(const auto& step:method["workflow"].Items()){check(AiUiText(step["step"].AsString(),true)!=step["step"].AsString()||step["step"].AsString()=="GPU","Workflow headings localized");check(AiUiText(step["rule"].AsString(),true)!=step["rule"].AsString(),"Workflow instructions localized");}
    check(AiUiText("Shader name supplied by trace",true)=="Shader name supplied by trace","Trace data not translated");
    bool methodView=false;for(const auto& op:catalog["operations"].Items())if(op["id"].AsString()=="focus_view")for(const auto& name:op["inputSchema"]["properties"]["view"]["enum"].Items())methodView|=name.AsString()=="ai_method";
    check(methodView,"Native standalone method view discoverable in the shared contract");
    std::cout<<checks<<" checks, "<<failed<<" failures\n";return failed?1:0;
}

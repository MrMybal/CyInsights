// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "../Quest/CyInsightOVR/experimental/GraphHudData.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv){
    int checks=0;auto check=[&](bool okay,const char* why){++checks;if(!okay)throw std::runtime_error(why);};
    cygraphs::Sample sample;sample.position=4;sample.stamp=20000000000;sample.curves.resize(2);
    std::strcpy(sample.curves[0].title.data(),"DRAW CALLS/S");std::strcpy(sample.curves[0].value.data(),"1250 DRAW/S");std::strcpy(sample.curves[1].title.data(),"TRIANGLES/S");std::strcpy(sample.curves[1].value.data(),"UNAVAILABLE");
    for(size_t i=0;i<cygraphs::Points;++i){sample.curves[0].points[i]=float(i)/float(cygraphs::Points-1);sample.curves[1].points[i]=-1;}
    auto packet=[&](uint32_t version){std::vector<char> out(32+sample.curves.size()*(64+cygraphs::Points*4+(version==2?4:0)));size_t at=0;auto write=[&](auto value){std::memcpy(out.data()+at,&value,sizeof(value));at+=sizeof(value);};write(uint32_t(0x43594731));write(version);write(sample.position);write(sample.size);write(sample.opacity);write(sample.stamp);write(uint32_t(sample.curves.size()));for(auto c:sample.curves){if(version==2)write(c.flags);write(c.title);write(c.value);write(c.points);}return out;};
    auto bytes=packet(2);check(bool(cygraphs::Parse(packet(1))),"Previous graph packet stays readable");
    auto parsed=cygraphs::Parse(bytes);check(bool(parsed)&&parsed->curves.size()==2,"Curve packet decode");
    auto geometry=cygraphs::Geometry(*parsed,2016,1760);check(!geometry.empty()&&geometry.size()%3==0,"Batched triangle geometry");
    if(argc>2){std::ofstream preview(argv[2],std::ios::binary);preview.write(reinterpret_cast<const char*>(geometry.data()),geometry.size()*sizeof(cygraphs::Vertex));}
    for(const auto& vertex:geometry)check(std::isfinite(vertex.x)&&std::isfinite(vertex.y)&&vertex.a>0&&vertex.a<=1,"Finite transparent vertices");
    for(unsigned position=0;position<9;++position){sample.position=position;auto points=cygraphs::Geometry(sample,2016,1760);check(!points.empty(),"Every HUD placement supported");for(auto p:points)check(p.x>=-1&&p.x<=1&&p.y>=-1&&p.y<=1,"Geometry inside selected surface");}
    for(auto& curve:sample.curves)curve.flags=2;auto textOnly=cygraphs::Geometry(sample,2016,1760);check(!textOnly.empty()&&textOnly.size()<geometry.size(),"Text-only HUD omits graph lines");
    auto textPacket=cygraphs::Parse(packet(2));check(bool(textPacket)&&textPacket->curves[0].flags==2,"Text flag is preserved on the wire");
    auto height=[](const auto& points){float top=1,bottom=-1;for(auto p:points){top=std::min(top,p.y);bottom=std::max(bottom,p.y);}return bottom-top;};
    check(height(textOnly)<height(geometry),"Text-only panel uses compact rows");
    sample.curves[0].flags=0;check(!cygraphs::Parse(packet(2)),"Reject invisible metric flags");sample.curves[0].flags=4;check(!cygraphs::Parse(packet(2)),"Reject unknown metric flags");for(auto& curve:sample.curves)curve.flags=3;
    bytes.pop_back();check(!cygraphs::Parse(bytes),"Reject truncated payload");bytes.push_back(0);bytes[0]=0;check(!cygraphs::Parse(bytes),"Reject unrelated protocol");
    sample.curves[0].points.fill(-1);auto empty=cygraphs::Geometry(sample,2016,1760);check(empty.size()<geometry.size(),"Unavailable history produces no fabricated curve");
    check(cygraphs::Geometry(sample,128,128).empty(),"No HUD in small offscreen targets");
    if(argc>1){std::ifstream input(argv[1],std::ios::binary);std::vector<char> data((std::istreambuf_iterator<char>(input)),{});check(bool(cygraphs::Parse(data)),"Actual Java producer matches native reader");}
    std::cout<<checks<<" graph protocol, geometry and placement checks passed\n";
}

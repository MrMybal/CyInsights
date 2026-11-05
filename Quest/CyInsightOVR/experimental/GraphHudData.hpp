// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
namespace cygraphs {
constexpr size_t Points=56,Maximum=64;
struct Curve {std::array<char,32> title{},value{};std::array<float,Points> points{};uint32_t flags=3;};
struct Sample {uint32_t position=8,size=0;float opacity=.8f;uint64_t stamp=0;std::vector<Curve> curves;};
inline std::shared_ptr<const Sample> Parse(const std::vector<char>& bytes){
    if(bytes.size()<32)return {};size_t at=0;auto read=[&](auto& value){if(at+sizeof(value)>bytes.size())return false;std::memcpy(&value,bytes.data()+at,sizeof(value));at+=sizeof(value);return true;};
    uint32_t magic=0,version=0,count=0;auto out=std::make_shared<Sample>();
    if(!read(magic)||!read(version)||!read(out->position)||!read(out->size)||!read(out->opacity)||!read(out->stamp)||!read(count)||magic!=0x43594731||(version!=1&&version!=2)||out->position>8||out->size>1||!std::isfinite(out->opacity)||out->opacity<.1f||out->opacity>1||!count||count>Maximum||bytes.size()!=32+count*(64+Points*4+(version==2?4:0)))return {};
    out->curves.resize(count);for(auto& curve:out->curves){if(version==2&&(!read(curve.flags)||!curve.flags||curve.flags>3))return {};if(!read(curve.title)||!read(curve.value)||!read(curve.points))return {};for(auto* text:{&curve.title,&curve.value}){if(text->back()!=0)return {};for(char ch:*text)if(ch&& (ch<32||ch>126))return {};}
        for(float value:curve.points)if(!std::isfinite(value)||value< -1||value>1)return {};}
    return out;
}
struct Vertex {float x,y,r,g,b,a;};
using Color=std::array<float,4>;
inline void Rect(std::vector<Vertex>& out,float x,float y,float w,float h,Color c,float width,float height){
    if(w<=0||h<=0)return;float l=x/width*2-1,t=y/height*2-1,r=(x+w)/width*2-1,b=(y+h)/height*2-1;
    for(auto p:std::array<std::array<float,2>,6>{{{l,t},{r,t},{l,b},{l,b},{r,t},{r,b}}})out.push_back({p[0],p[1],c[0],c[1],c[2],c[3]});
}
// Our small 5x7 display alphabet. Rows are packed into five-bit columns.
inline uint64_t Glyph(char ch){
    constexpr char letters[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .%/+:_-?";
    constexpr uint64_t rows[]={0x3a31fc631ULL,0x7a31f463eULL,0x3e108420fULL,0x7a318c63eULL,0x7e10f421fULL,0x7e10f4210ULL,0x3e10bc62fULL,0x4631fc631ULL,0x7c842109fULL,0x084210a2eULL,0x4654c5251ULL,0x42108421fULL,0x4775ac631ULL,0x47359c631ULL,0x3a318c62eULL,0x7a31f4210ULL,0x3a318d66fULL,0x7a31f5251ULL,0x3e107043eULL,0x7c8421084ULL,0x46318c62eULL,0x46318c544ULL,0x4631ad771ULL,0x462a22a31ULL,0x462a21084ULL,0x7c222221fULL,
        0x3a33ae62eULL,0x11842108eULL,0x3a211111fULL,0x78217043eULL,0x08ca97c42ULL,0x7e10f043eULL,0x3a10f462eULL,0x7c2222108ULL,0x3a317462eULL,0x3a317842eULL,
        0,0x00000000cULL,0x673222667ULL,0x044222110ULL,0x0084f9080ULL,0x001800600ULL,0x00000001fULL,0x0000f8000ULL,0x3a2110084ULL};
    static_assert(sizeof(rows)/sizeof(rows[0])==sizeof(letters)-1,"Complete display alphabet");
    const char* found=std::strchr(letters,ch);return rows[found?size_t(found-letters):sizeof(rows)/sizeof(rows[0])-1];
}
inline void Text(std::vector<Vertex>& out,const char* text,float x,float y,float scale,Color color,float w,float h){
    for(size_t n=0;n<31&&text[n];++n){uint64_t glyph=Glyph(text[n]);for(int row=0;row<7;++row)for(int column=0;column<5;){
        if(!(glyph&(1ull<<(34-row*5-column)))){++column;continue;}int begin=column;while(column<5&&(glyph&(1ull<<(34-row*5-column))))++column;
        Rect(out,x+(n*6+begin)*scale,y+row*scale,(column-begin)*scale,scale,color,w,h);}}
}
inline std::vector<Vertex> Geometry(const Sample& sample,float width,float height){
    std::vector<Vertex> out;if(width<512||height<512||sample.curves.empty())return out;
    size_t columns=sample.curves.size()>4?2:1,rows=(sample.curves.size()+columns-1)/columns;
    std::vector<float> rowHeights(rows,0);float total=0;for(size_t i=0;i<sample.curves.size();++i)rowHeights[i/columns]=std::max(rowHeights[i/columns],float(sample.curves[i].flags&1?(sample.size?115:90):(sample.size?40:32)));for(float h:rowHeights)total+=h;
    float w=std::min(width*.52f,sample.size?780.f:600.f),h=std::min(height*.65f,total),marginX=width*.12f,marginY=height*.12f;
    auto p=sample.position;float x=(p==0||p==2||p==5)?marginX:(p==1||p==3||p==7)?width-w-marginX:(width-w)*.5f;
    float y=(p==0||p==1||p==4)?marginY:(p==2||p==3||p==8)?height-h-marginY:(height-h)*.5f;
    Rect(out,x-6,y-6,w+12,h+12,{.015f,.025f,.045f,sample.opacity*.55f},width,height);
    float cellW=w/columns,font=std::min(1.4f,cellW/195.f);
    for(size_t c=0;c<sample.curves.size();++c){const auto& curve=sample.curves[c];float rowY=0;for(size_t row=0;row<c/columns;++row)rowY+=rowHeights[row]*h/total;
        float cellH=rowHeights[c/columns]*h/total,left=x+(c%columns)*cellW+5,top=y+rowY;
        Color color=c%3==0?Color{.2f,.9f,1,sample.opacity}:c%3==1?Color{1,.55f,.9f,sample.opacity}:Color{.55f,1,.4f,sample.opacity};
        Text(out,curve.title.data(),left,top,font,color,width,height);if(curve.flags&2)Text(out,curve.value.data(),left,top+9*font,font,{1,1,1,sample.opacity},width,height);
        if(!(curve.flags&1))continue;float caption=(curve.flags&2)?19.f:10.f;
        float graphY=top+caption*font,graphH=std::max(3.f,cellH-(caption+4)*font),graphW=cellW-12;
        Rect(out,left,graphY+graphH,graphW,1,{.4f,.5f,.6f,sample.opacity*.4f},width,height);
        for(size_t i=1;i<Points;++i){float a=curve.points[i-1],b=curve.points[i];if(a<0||b<0)continue;
            float ax=left+(i-1)*graphW/(Points-1),bx=left+i*graphW/(Points-1),ay=graphY+(1-a)*graphH,by=graphY+(1-b)*graphH;
            float dx=bx-ax,dy=by-ay,length=std::sqrt(dx*dx+dy*dy),nx=-dy/length,ny=dx/length;
            std::array<std::array<float,2>,4> v{{{ax+nx,ay+ny},{bx+nx,by+ny},{ax-nx,ay-ny},{bx-nx,by-ny}}};
            for(int k:{0,1,2,2,1,3})out.push_back({v[k][0]/width*2-1,v[k][1]/height*2-1,color[0],color[1],color[2],color[3]});
        }
    }return out;
}
}

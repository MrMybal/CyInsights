// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "../Quest/CyInsightOVR/experimental/DrawCounters.hpp"
#include <iostream>
int main(){
    using namespace cydraw;int checks=0,failed=0;
    auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<why<<'\n';}};
    check(Direct(3,false,true,300,4).triangles==400,"Indexed instancing");
    check(Direct(4,false,false,5,2).triangles==6,"Triangle strip");
    check(Direct(5,false,false,5,3).triangles==9,"Triangle fan");
    check(Direct(8,false,true,12,2).triangles==4,"List with adjacency");
    check(Direct(9,false,false,10,1).triangles==3,"Strip with adjacency");
    check(Direct(1,false,false,10,1).triangles==0&&Direct(1,false,false,10,1).unknownTriangles==0,"Lines are not triangles");
    check(Direct(3,false,false,2,100).triangles==0,"Incomplete primitive");
    check(Direct(3,false,true,30,0).triangles==0,"No instances");
    check(Direct(4,true,true,30,1).unknownTriangles==1,"Restart indices must not be guessed");
    check(Direct(4,true,false,30,1).triangles==28,"Restart does not affect non-indexed drawing");
    check(Direct(-1,false,false,30,1).unknownTriangles==1,"Unknown dynamic topology");
    check(Direct(10,false,false,30,1).unknownTriangles==1,"Patch output is not inferred");
    auto indirect=Indirect(8);check(indirect.draws==8&&indirect.unknownTriangles==8&&indirect.indirect==1,"Fixed indirect counts preserve incomplete geometry");
    auto controlled=Indirect(999,true);check(controlled.draws==0&&controlled.unknownDraws==1,"GPU count is not the maximum");
    Book book;book.Begin(1);book.Draw(1,Direct(3,false,false,12,1));
    check(book.Submit({1}).triangles==4,"Submitted primary");
    check(book.Submit({1,1}).draws==2&&book.Submit({1,1}).triangles==8,"Reused command buffers counted per execution");
    book.Begin(2);book.Draw(2,Direct(3,false,true,6,5));book.Execute(1,2);book.Execute(1,2);
    check(book.Submit({1}).triangles==24&&book.Submit({1}).draws==3,"Repeated secondary executions");
    book.Erase(2);check(book.Submit({1}).missingBuffers==2,"Freed secondary cannot become an exact zero");
    book.Begin(1);check(book.Submit({1}).draws==0,"Reset/rerecord clears old draws");
    book.Execute(1,1);check(book.Submit({1}).missingBuffers==1,"Invalid recursion bounded");
    check(book.Submit({999}).missingBuffers==1,"Unknown primary remains incomplete");
    Counts overflow;overflow.draws=UINT64_MAX;Counts one;one.draws=1;overflow+=one;check(overflow.overflow&&overflow.draws==UINT64_MAX,"Counter saturation");
    std::cout<<checks<<" submitted-geometry checks, "<<failed<<" failures\n";return failed?1:0;
}

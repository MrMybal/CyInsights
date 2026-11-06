// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "../Common/Documentation/HelpContract.hpp"
#include <iostream>
#include <cstdlib>
int main(){
    auto check=[](bool ok){if(!ok){std::cerr<<"CyDoc route check failed\n";std::exit(1);}};
    check(cyhelp::Route("file:///C:/Program%20Files/Cy/Documentation/Documentation.html","en","timeline","section-1")=="file:///C:/Program%20Files/Cy/Documentation/Documentation.html#/en/timeline/section-1");
    check(cyhelp::Route("file:///Documentation/Documentation.html#old","fr","quest-hud",{},"a b")=="file:///Documentation/Documentation.html#/fr/quest-hud?media=a%20b");
    check(cyhelp::Route("file:///Documentation/Documentation.html","de",{})=="file:///Documentation/Documentation.html#/en/welcome");
    check(cyhelp::Route("file:///Documentation/Documentation.html","en","a/#?é").find("a%2F%23%3F%C3%A9")!=std::string::npos);
    std::cout<<"CyDoc encoded native routes passed\n";
}

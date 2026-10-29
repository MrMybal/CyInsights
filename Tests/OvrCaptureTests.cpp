// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "../Core/OvrCapture.hpp"
#include <iostream>
int main(){
    try {
        cygi::Json j;if(!cygi::Json::Parse(R"({"schema":"cyinsightovr.capture","version":1,"source":"meta.ovrgpuprofiler","time_basis":"capture_relative_unanchored","capture_id":"fixture","android_boot_id":"fixture-boot","observed_mono_ns":"100","target_package":"org.test.game","status":"measured","surfaces":[{"id":"1/1","process":"org.test.game","width":100,"height":80,"tile_width":64,"tile_height":64,"msaa":4,"duration_us":400,"tiles":[{"id":0,"x":0,"y":0,"width":64,"height":64,"stages_us":{"Render":0,"Binning":12}}]}]})",j))return 1;
        auto c=cyovr::ParseCapture(j);if(c.surfaces.size()!=1||c.surfaces[0].tiles[0].Cost("Render")!=0||!std::isnan(c.surfaces[0].tiles[0].Cost("StoreColor")))return 2;
        auto rejects=[](const cygi::Json& v){try{cyovr::ParseCapture(v);return false;}catch(...){return true;}};
        auto invalid=j;invalid["version"]=2;if(!rejects(invalid))return 3;
        invalid=j;invalid["time_basis"]="unreal.frame";if(!rejects(invalid))return 4;
        invalid=j;invalid["target_package"]="org.other.game";if(!rejects(invalid))return 5;
        invalid=j;invalid["surfaces"]=cygi::Json::Array();if(!rejects(invalid))return 6;
        invalid=j;invalid["capture_id"]="";if(!rejects(invalid))return 7;
        invalid=j;auto empty_surface=j["surfaces"].Items().front();empty_surface["tiles"]=cygi::Json::Array();
        invalid["surfaces"]=cygi::Json::Array();invalid["surfaces"].Push(empty_surface);if(!rejects(invalid))return 8;
        std::cout<<"OVR measured geometry, availability, target and clock validation passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 20;}
}

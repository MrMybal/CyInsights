// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import org.json.*;
import java.io.*;
import java.util.*;
/** Explicit simulated service tests; they do not prove headset visibility. */
public final class MetaHudTests {
    private static final class Fake implements MetaHudController.Runner {
        JSONObject config;boolean failOnce=false;int calls=0;
        Fake()throws Exception{config=new JSONObject().put("overlayEnabled",false).put("statsEnabled",true).put("graphEnabled",true)
            .put("overlayScale",1.5).put("overlayDistance",.6).put("overlayPitch",20).put("overlayYaw",0).put("overlayHeadLocked",true)
            .put("metrics",new JSONArray().put(new JSONObject().put("name","Average FPS").put("graphEnabled",true).put("statEnabled",true)).put(new JSONObject().put("name","App GPU Time (us)").put("graphEnabled",false).put("statEnabled",true)).put(new JSONObject().put("name","Untouched metric").put("graphEnabled",false).put("statEnabled",true))
                .put(new JSONObject().put("name","CPU Utilization").put("graphEnabled",false).put("statEnabled",false)));}
        public String run(String[] args,int timeout)throws Exception{
            ++calls;if(args[0].endsWith("pm"))return "package:/system/example/base.apk";
            if(args[0].endsWith("logcat"))return "I/SettingsBroadcastReceiver.Info."+System.currentTimeMillis()+"( 123): "+config.toString()+"\n";
            String action=args[5].substring(args[5].lastIndexOf('.')+1);
            if(action.equals("ENABLE_STATS"))config.put("statsEnabled",true);
            if(action.equals("DISABLE_STATS"))config.put("statsEnabled",false);
            if(action.equals("ENABLE_GRAPH")||action.equals("DISABLE_GRAPH")){
                boolean enabled=action.equals("ENABLE_GRAPH");
                if(args.length==6)config.put("graphEnabled",enabled);
                else selected(args[8]).put("graphEnabled",enabled);
            }
            if(action.equals("ENABLE_STAT")||action.equals("DISABLE_STAT"))selected(args[8]).put("statEnabled",action.equals("ENABLE_STAT"));
            if(action.equals("DISABLE_OVERLAY"))config.put("overlayEnabled",false);
            if(action.equals("ENABLE_OVERLAY")){
                if(failOnce){failOnce=false;throw new IOException("Simulated activation failure");}
                config.put("overlayEnabled",true);
                // The device may reapply persisted table visibility here.
                config.put("statsEnabled",true);
                for(int i=6;i+2<args.length;i+=3){String k=args[i+1],value=args[i+2];
                    if(k.equals("headlocked"))config.put("overlayHeadLocked",Boolean.parseBoolean(value));
                    else config.put(k.equals("scale")?"overlayScale":k.equals("distance")?"overlayDistance":k.equals("pitch")?"overlayPitch":"overlayYaw",Double.parseDouble(value));}
            }
            return "Broadcast completed: result=0";
        }
        private JSONObject selected(String id)throws Exception{for(String[] d:MetaHudController.METRICS)if(d[0].equals(id))return MetaHudController.metric(config,d[1]);throw new AssertionError("Undocumented metric sent to service");}
    }
    private static void check(boolean condition,String message){if(!condition)throw new AssertionError(message);}
    public static void main(String[] args)throws Exception{
        Fake fake=new Fake();String json=fake.config.toString();int middle=json.length()/2;
        String chunks="I/SettingsBroadcastReceiver.Info.100( 123): Start::\nI/SettingsBroadcastReceiver.Info.100( 123): "+json.substring(0,middle)+"\nI/SettingsBroadcastReceiver.Info.100( 123): "+json.substring(middle)+"\nI/SettingsBroadcastReceiver.Info.100( 123): End::\n";
        check(MetaHudController.parseState(chunks,100).getDouble("overlayScale")==1.5,"Chunked LOG_STATE");
        try{MetaHudController.parseState(chunks,101);throw new AssertionError("Stale state accepted");}catch(JSONException expected){}
        File file=new File("/data/local/tmp/cyinsightovr-hud-test-"+android.os.Process.myPid()+".json");
        if(file.exists())throw new IOException("Test state already exists");
        try{
            MetaHudController hud=new MetaHudController(fake,file);int before=fake.calls;
            try{hud.enable("huge","upper_left");throw new AssertionError("Invalid size accepted");}catch(IllegalArgumentException expected){}
            check(fake.calls==before&&!file.exists(),"Validate options before changing service");
            String originalGraphs=fake.config.getJSONArray("metrics").toString();JSONObject response=hud.enable("small","upper_left");
            check(response.getBoolean("settings_verified")&&fake.config.getDouble("overlayScale")==1&&fake.config.getDouble("overlayDistance")==2&&!fake.config.getBoolean("statsEnabled"),"Compact placement and table hidden");
            check(fake.config.getJSONArray("metrics").getJSONObject(1).getBoolean("graphEnabled")&&!fake.config.getJSONArray("metrics").getJSONObject(2).getBoolean("graphEnabled"),"GPU graph enabled without changing unrelated graphs");
            hud.disable();check(!file.exists()&&!fake.config.getBoolean("overlayEnabled")&&fake.config.getBoolean("statsEnabled")&&fake.config.getDouble("overlayPitch")==20&&fake.config.getDouble("overlayDistance")==.6&&fake.config.getJSONArray("metrics").toString().equals(originalGraphs),"Original configuration and graph selections restored");
            hud.enable("medium","lower_right");new MetaHudController(fake,file).disable();check(!file.exists()&&fake.config.getDouble("overlayScale")==1.5,"Helper restart restores saved settings");
            fake.failOnce=true;try{hud.enable("small","upper_left");throw new AssertionError("Failure not propagated");}catch(IOException expected){}
            check(!file.exists()&&!fake.config.getBoolean("overlayEnabled")&&fake.config.getBoolean("statsEnabled"),"Failed activation rolls back");
            JSONObject selected=new JSONObject().put("graphs",new JSONArray().put("cpu_utilization_percentage")).put("stats",new JSONArray().put("average_frame_rate"));
            hud.enable(selected);
            check(fake.config.getBoolean("statsEnabled")&&fake.config.getJSONArray("metrics").getJSONObject(3).getBoolean("graphEnabled")&&!fake.config.getJSONArray("metrics").getJSONObject(0).getBoolean("graphEnabled")&&fake.config.getJSONArray("metrics").getJSONObject(0).getBoolean("statEnabled")&&!fake.config.getJSONArray("metrics").getJSONObject(1).getBoolean("statEnabled"),"Independent graph/value selection");
            hud.disable();check(fake.config.getJSONArray("metrics").toString().equals(originalGraphs),"Selected metrics restored exactly");
            before=fake.calls;
            try{hud.enable(new JSONObject().put("graphs",new JSONArray().put("unknown")));throw new AssertionError("Unknown metric accepted");}catch(IllegalArgumentException expected){}
            check(fake.calls==before&&!file.exists(),"Unknown metric rejected before mutations");
            try{hud.enable(new JSONObject().put("graphs",new JSONArray()).put("stats",new JSONArray()));throw new AssertionError("Empty HUD accepted");}catch(IllegalArgumentException expected){}
            JSONObject catalog=hud.catalog();check(catalog.getJSONArray("metrics").length()==MetaHudController.METRICS.length&&!catalog.getJSONArray("metrics").getJSONObject(2).getBoolean("available"),"Firmware availability is explicit");
            hud.enable("small","upper_center");check(fake.config.getDouble("overlayYaw")==0&&fake.config.getDouble("overlayPitch")==8,"Top center placement");hud.disable();
            hud.enable("small","center");check(fake.config.getDouble("overlayYaw")==0&&fake.config.getDouble("overlayPitch")==0,"Centered placement");hud.disable();
            hud.enable("small","lower_center");check(fake.config.getDouble("overlayYaw")==0&&fake.config.getDouble("overlayPitch")==-8,"Bottom center placement");hud.disable();
            System.out.println("16 Meta HUD checks passed (explicit simulated service)");
        }finally{if(file.exists())file.delete();new File(file.getPath()+".tmp").delete();}
    }
}

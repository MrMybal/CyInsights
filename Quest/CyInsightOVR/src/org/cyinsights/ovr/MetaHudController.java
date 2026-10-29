// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import org.json.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.regex.*;

/** Documented Meta HUD controls. No private compositor permission is assumed. */
final class MetaHudController {
    interface Runner {String run(String[] args,int timeout)throws Exception;}
    private final Runner runner;private final File saved;
    private JSONObject baseline;private boolean requested;
    static final String[][] METRICS={
        {"average_frame_rate","Average FPS"},{"app_gpu_time_microseconds","App GPU Time"},
        {"timewarp_gpu_time_microseconds","Timewarp GPU Time"},{"cpu_utilization_percentage","CPU Utilization"},
        {"gpu_utilization_percentage","GPU Utilization"},{"cpu_frequency_MHz","CPU Frequency"},
        {"gpu_frequency_MHz","GPU Frequency"},{"battery_level_percentage","Battery Level"},
        {"battery_temperature_celcius","Battery Temperature"},{"available_memory_MB","Available Memory"},
        {"stale_frame_count","Stale Frame Count"},{"display_refresh_rate","Display Refresh Rate"},
        {"percent_time_shading_frags","Percent of Time Shading Fragments"},{"percent_time_shading_verts","Percent of Time Shading Vertices"},
        {"app_pss_MB","Used Memory (MB) [PSS]"},{"cpu_level","CPU Level"},{"gpu_level","GPU Level"},
        {"early_frame_count","Early Frame Count"},{"foveation_level","Foveation Level"},
        {"eye_buffer_width","Eye Buffer Width"},{"eye_buffer_height","Eye Buffer Height"},
        {"spacewarped_frames_per_second","SpaceWarp FPS"},{"app_vss_MB","Used Memory (MB) [VSS]"},
        {"app_rss_MB","Used Memory (MB) [RSS]"},{"app_uss_MB","Used Memory (MB) [USS]"},
        {"app_private_dirty_MB","Private Dirty Memory (MB)"},{"app_private_clean_MB","Private Clean Memory (MB)"},
        {"stale_frames_consecutive","Max Consecutive Stale Frames"},{"vrshell_average_frame_rate","VrShell Average Frame Rate"},
        {"vrshell_and_guardian_gpu_time_microseconds","VrShell + Boundary GPU Time"}};
    MetaHudController(Runner runner,File saved){this.runner=runner;this.saved=saved;}
    private void broadcast(String action,String... extras)throws Exception{
        ArrayList<String> args=new ArrayList<>(Arrays.asList("/system/bin/am","broadcast","-n",
            "com.oculus.ovrmonitormetricsservice/.SettingsBroadcastReceiver","-a","com.oculus.ovrmonitormetricsservice."+action));
        Collections.addAll(args,extras);runner.run(args.toArray(new String[0]),5);
    }
    /** LOG_STATE may be a single JSON line or Start:: / chunks / End::. */
    static JSONObject parseState(String text,long notBefore)throws JSONException{
        Pattern prefix=Pattern.compile("^[VDIWEF]/SettingsBroadcastReceiver\\.Info\\.(\\d+)\\(\\s*\\d+\\): ?(.*)$");
        TreeMap<Long,StringBuilder> chunks=new TreeMap<>();
        for(String line:text.split("[\\r\\n]+")){
            Matcher match=prefix.matcher(line);if(!match.matches())continue;
            long stamp;try{stamp=Long.parseLong(match.group(1));}catch(NumberFormatException e){continue;}
            if(stamp<notBefore)continue;String part=match.group(2);
            if(part.equals("Start::")||part.equals("End::"))continue;
            StringBuilder value=chunks.get(stamp);if(value==null){value=new StringBuilder();chunks.put(stamp,value);}
            if(value.length()+part.length()<128*1024)value.append(part);
        }
        for(StringBuilder value:chunks.descendingMap().values()){
            try{JSONObject state=new JSONObject(value.toString());
                if(state.has("overlayEnabled")&&state.has("overlayScale")&&state.has("overlayDistance")&&state.has("statsEnabled")&&state.has("overlayPitch")&&state.has("overlayYaw")&&state.has("overlayHeadLocked"))return state;
            }catch(JSONException ignored){}
        }
        throw new JSONException("No complete fresh Meta HUD configuration received");
    }
    private JSONObject readState()throws Exception{
        long start=System.currentTimeMillis();broadcast("LOG_STATE");
        for(int i=0;i<4;++i){
            String logs=runner.run(new String[]{"/system/bin/logcat","-d","-v","brief","-t","500"},3);
            try{return parseState(logs,start);}catch(JSONException e){if(i==3)throw e;Thread.sleep(100);}
        }
        throw new IOException("Meta HUD configuration unavailable");
    }
    private void load()throws Exception{
        if(baseline!=null||!saved.exists())return;
        if(saved.length()>128*1024)throw new IOException("Invalid saved Meta HUD configuration");
        try(BufferedReader reader=new BufferedReader(new FileReader(saved))){baseline=new JSONObject(reader.readLine());}
    }
    private void store(JSONObject state)throws Exception{
        File temporary=new File(saved.getPath()+".tmp");
        if(temporary.exists()&&!temporary.delete())throw new IOException("Cannot replace saved HUD state");
        if(!temporary.createNewFile()||!temporary.setReadable(false,false)||!temporary.setWritable(false,false)||!temporary.setReadable(true,true)||!temporary.setWritable(true,true))throw new IOException("Cannot protect saved HUD state");
        try(FileOutputStream out=new FileOutputStream(temporary)){out.write(state.toString().getBytes(StandardCharsets.UTF_8));}
        if(!temporary.renameTo(saved))throw new IOException("Cannot save original HUD configuration");
        baseline=state;
    }
    private void position(JSONObject state)throws Exception{
        broadcast("ENABLE_OVERLAY","--ez","headlocked",Boolean.toString(state.getBoolean("overlayHeadLocked")),
            "--ef","pitch",Double.toString(state.getDouble("overlayPitch")),"--ef","yaw",Double.toString(state.getDouble("overlayYaw")),
            "--ef","scale",Double.toString(state.getDouble("overlayScale")),"--ef","distance",Double.toString(state.getDouble("overlayDistance")));
    }
    static JSONObject metric(JSONObject state,String prefix)throws JSONException{
        JSONArray metrics=state.getJSONArray("metrics");
        for(int i=0;i<metrics.length();++i){JSONObject metric=metrics.getJSONObject(i);String name=metric.optString("name");if(name.equals(prefix)||name.startsWith(prefix+" ("))return metric;}
        throw new JSONException("Meta HUD metric unavailable: "+prefix);
    }
    private void graph(String id,boolean enabled)throws Exception{broadcast(enabled?"ENABLE_GRAPH":"DISABLE_GRAPH","--es","stat",id);}
    private void stat(String id,boolean enabled)throws Exception{broadcast(enabled?"ENABLE_STAT":"DISABLE_STAT","--es","stat",id);}
    synchronized JSONObject catalog()throws Exception{
        JSONObject observed=readState();JSONArray items=new JSONArray();
        for(String[] descriptor:METRICS){boolean available=true;try{metric(observed,descriptor[1]);}catch(JSONException missing){available=false;}
            items.put(new JSONObject().put("id",descriptor[0]).put("title",descriptor[1]).put("available",available));}
        return new JSONObject().put("metrics",items).put("backend","meta.ovrmetrics.documented_hud");
    }
    private static Set<String> selection(JSONArray ids,int maximum)throws JSONException{
        if(ids==null||ids.length()>maximum)throw new IllegalArgumentException("Too many or invalid HUD selections");
        Set<String> out=new LinkedHashSet<>();
        for(int i=0;i<ids.length();++i){String id=ids.getString(i);boolean known=false;for(String[] descriptor:METRICS)known|=descriptor[0].equals(id);
            if(!known||!out.add(id))throw new IllegalArgumentException("Unknown or duplicate HUD metric");}
        return out;
    }
    private void restore()throws Exception{
        load();if(baseline==null){broadcast("DISABLE_OVERLAY");requested=false;return;}
        // Restore each managed selection as well as global placement/visibility.
        // Metrics outside our documented catalog are never modified.
        position(baseline);broadcast(baseline.getBoolean("statsEnabled")?"ENABLE_STATS":"DISABLE_STATS");
        if(baseline.has("metrics")){
            for(String[] descriptor:METRICS){JSONObject item;try{item=metric(baseline,descriptor[1]);}catch(JSONException absent){continue;}
                graph(descriptor[0],item.getBoolean("graphEnabled"));if(item.has("statEnabled"))stat(descriptor[0],item.getBoolean("statEnabled"));}
        }
        if(baseline.has("graphEnabled"))broadcast(baseline.getBoolean("graphEnabled")?"ENABLE_GRAPH":"DISABLE_GRAPH");
        if(!baseline.getBoolean("overlayEnabled"))broadcast("DISABLE_OVERLAY");
        if(saved.exists()&&!saved.delete())throw new IOException("Original HUD settings restored, but snapshot cleanup failed");
        baseline=null;requested=false;
    }
    synchronized JSONObject enable(String size,String location)throws Exception{
        return enable(new JSONObject().put("size",size).put("corner",location));
    }
    synchronized JSONObject enable(JSONObject request)throws Exception{
        String size=request.optString("size","small"),location=request.optString("corner","upper_left");
        if(!size.equals("small")&&!size.equals("medium"))throw new IllegalArgumentException("HUD size must be small or medium");
        if(!Arrays.asList("upper_left","upper_right","lower_left","lower_right","upper_center","center_left","center","center_right","lower_center").contains(location))throw new IllegalArgumentException("Invalid HUD position");
        Set<String> graphs=selection(request.has("graphs")?request.getJSONArray("graphs"):new JSONArray().put("average_frame_rate").put("app_gpu_time_microseconds"),6);
        Set<String> stats=selection(request.has("stats")?request.getJSONArray("stats"):new JSONArray(),8);
        if(graphs.isEmpty()&&stats.isEmpty())throw new IllegalArgumentException("Choose at least one HUD graph or value");
        if(!runner.run(new String[]{"/system/bin/pm","path","com.oculus.ovrmonitormetricsservice"},3).contains("package:"))throw new IOException("Meta's VR HUD service is not installed");
        load();JSONObject current=readState();
        for(String[] descriptor:METRICS)if(graphs.contains(descriptor[0])||stats.contains(descriptor[0]))metric(current,descriptor[1]);
        if(baseline==null)store(current);
        try{
            JSONObject compact=new JSONObject().put("overlayHeadLocked",true).put("overlayScale",size.equals("small")?1:1.4)
                .put("overlayDistance",2).put("overlayPitch",location.startsWith("upper")?8:location.startsWith("lower")?-8:0).put("overlayYaw",location.endsWith("left")?-12:location.endsWith("right")?12:0);
            position(compact);
            for(String[] descriptor:METRICS){JSONObject item;try{item=metric(current,descriptor[1]);}catch(JSONException absent){continue;}
                if(item.optBoolean("graphEnabled")!=graphs.contains(descriptor[0]))graph(descriptor[0],graphs.contains(descriptor[0]));
                if(item.optBoolean("statEnabled")!=stats.contains(descriptor[0]))stat(descriptor[0],stats.contains(descriptor[0]));}
            broadcast(graphs.isEmpty()?"DISABLE_GRAPH":"ENABLE_GRAPH");
            // ENABLE_OVERLAY can reapply persisted display settings. Hide the
            // tall table only after placement, then verify settled state.
            broadcast(stats.isEmpty()?"DISABLE_STATS":"ENABLE_STATS");Thread.sleep(200);JSONObject observed=readState();
            if(!observed.optBoolean("overlayEnabled")||observed.optBoolean("statsEnabled")!=!stats.isEmpty()||observed.optBoolean("graphEnabled")!=!graphs.isEmpty()||!observed.optBoolean("overlayHeadLocked")||Math.abs(observed.optDouble("overlayPitch")-compact.getDouble("overlayPitch"))>.05||Math.abs(observed.optDouble("overlayYaw")-compact.getDouble("overlayYaw"))>.05||Math.abs(observed.optDouble("overlayDistance")-2)>.05||Math.abs(observed.optDouble("overlayScale")-compact.getDouble("overlayScale"))>.05)
                throw new IOException("Meta HUD did not accept the compact placement");
            for(String[] descriptor:METRICS){JSONObject item;try{item=metric(observed,descriptor[1]);}catch(JSONException absent){continue;}
                if(item.optBoolean("graphEnabled")!=graphs.contains(descriptor[0])||item.optBoolean("statEnabled")!=stats.contains(descriptor[0]))throw new IOException("Meta HUD did not accept metric selection: "+descriptor[1]);}
            requested=true;return state().put("accepted",true).put("size",size).put("corner",location).put("settings_verified",true)
                .put("graphs",new JSONArray(graphs)).put("stats",new JSONArray(stats));
        }catch(Exception e){try{restore();}catch(Exception restoration){e.addSuppressed(restoration);}throw e;}
    }
    synchronized JSONObject disable()throws Exception{restore();return state().put("accepted",true).put("settings_restored",true);}
    synchronized JSONObject state()throws JSONException{
        return new JSONObject().put("backend","meta.ovrmetrics.documented_hud").put("requested",requested)
            .put("visible_in_headset","requires_visual_confirmation").put("custom_renderer",false);
    }
}

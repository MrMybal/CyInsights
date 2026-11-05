// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import java.io.*;
import java.nio.*;
import java.util.*;
import org.json.*;
public final class GraphHudTests {
    static int checks;static void check(boolean value,String why){++checks;if(!value)throw new AssertionError(why);}
    static JSONObject spec(String name)throws Exception{return new JSONObject().put("name",name).put("title",name).put("unit","draw/s").put("scale",1);}
    public static void main(String[] args)throws Exception {
        GraphHudController graphs=new GraphHudController();long now=20000000000L;
        JSONArray selected=new JSONArray().put(spec("Vulkan Known Draw Calls/s")).put(spec("Vulkan Known Input Triangles/s"));
        JSONObject request=new JSONObject().put("package","org.cyinsights.gputest").put("custom_graphs",selected).put("custom_position",4).put("hud_size",0);
        check(graphs.start(request).getBoolean("active"),"Draw and triangle curves start together");
        for(int i=0;i<11;++i)graphs.sample(new JSONObject().put("name","Vulkan Known Draw Calls/s").put("observed_mono_ns",Long.toString(now-10000000000L+i*1000000000L)).put("value",100+i*10));
        byte[] packet=graphs.packet("org.cyinsights.gputest",now);ByteBuffer wire=ByteBuffer.wrap(packet).order(ByteOrder.LITTLE_ENDIAN);
        check(wire.getInt()==0x43594731&&wire.getInt()==2,"Versioned curve/text protocol");check(wire.getInt()==4,"Top-center placement");wire.position(28);check(wire.getInt()==2,"Both selected curves serialized");
        check(wire.getInt()==3,"Existing selection retains curves and values");
        wire.position(32+68);boolean measured=false;for(int i=0;i<GraphHudController.POINTS;++i)measured|=wire.getFloat()>=0;check(measured,"Measured draw history is sent");
        wire.position(32+68+GraphHudController.POINTS*4+68);boolean missing=true;for(int i=0;i<GraphHudController.POINTS;++i)missing&=wire.getFloat()==-1;check(missing,"Missing triangles are unavailable, never zero");
        check(graphs.status(now).getInt("measured_count")==1&&graphs.status(now+4000000000L).getInt("measured_count")==0,"Fresh and stale measurements are distinguished in the UI");
        JSONObject textRequest=new JSONObject(request.toString()).put("custom_graphs",new JSONArray().put(spec("Vulkan Known Draw Calls/s").put("graph",false).put("value",true)));
        graphs.start(textRequest);check(graphs.status(now).getJSONArray("graphs").length()==0&&graphs.status(now).getJSONArray("values").length()==1,"Text-only HUD is independent from graph selection");
        check(ByteBuffer.wrap(graphs.packet("org.cyinsights.gputest",now)).order(ByteOrder.LITTLE_ENDIAN).getInt(32)==2,"Text-only flag reaches the native renderer");
        JSONObject graphRequest=new JSONObject(textRequest.toString()).put("custom_graphs",new JSONArray().put(spec("Vulkan Known Draw Calls/s").put("graph",true).put("value",false)));
        graphs.start(graphRequest);check(ByteBuffer.wrap(graphs.packet("org.cyinsights.gputest",now)).order(ByteOrder.LITTLE_ENDIAN).getInt(32)==1,"Curve-only flag reaches native renderer");
        try{graphs.start(new JSONObject(textRequest.toString()).put("custom_graphs",new JSONArray().put(spec("bad").put("graph",false).put("value",false))));throw new AssertionError("Invisible metric accepted");}catch(IllegalArgumentException expected){++checks;}
        graphs.start(request);packet=graphs.packet("org.cyinsights.gputest",now);
        check(graphs.packet("another.target",now).length==0,"Selected-target isolation");
        check(graphs.packet("org.cyinsights.gputest",now+1)==packet,"Unchanged histories reuse packet preparation");
        GraphHudController peaks=new GraphHudController();peaks.start(request);
        for(long[] sample:new long[][]{{1000000000L,999999},{19010000000L,900},{19020000000L,10},{20000000000L,10}})peaks.sample(new JSONObject().put("name","Vulkan Known Draw Calls/s").put("observed_mono_ns",Long.toString(sample[0])).put("value",sample[1]));
        wire=ByteBuffer.wrap(peaks.packet("org.cyinsights.gputest",now)).order(ByteOrder.LITTLE_ENDIAN);wire.position(32+36);byte[] caption=new byte[32];wire.get(caption);check(new String(caption,"US-ASCII").contains("MAX 900"),"Visible peak survives downsampling; old spikes do not change scale");
        wire.position(32+68);float highest=-1;for(int i=0;i<GraphHudController.POINTS;++i)highest=Math.max(highest,wire.getFloat());check(highest==1,"Brief submission spike stays visible in curve");
        graphs.clearDrawHistory();byte[] reset=graphs.packet("org.cyinsights.gputest",now);wire=ByteBuffer.wrap(reset).order(ByteOrder.LITTLE_ENDIAN);wire.position(32+68);check(wire.getFloat()==-1,"New counter session discards old history");
        graphs.stop();check(graphs.packet("org.cyinsights.gputest",now).length==0,"Stop immediately hides curves");check(graphs.start(request).getBoolean("active"),"Restart does not need a game relaunch");
        try{graphs.start(new JSONObject(request.toString()).put("custom_position",99));throw new AssertionError("Bad position accepted");}catch(IllegalArgumentException expected){++checks;}
        try{graphs.start(new JSONObject(request.toString()).put("custom_graphs",new JSONArray().put(spec("duplicate")).put(spec("duplicate"))));throw new AssertionError("Duplicate accepted");}catch(IllegalArgumentException expected){++checks;}
        ArrayList<String> calls=new ArrayList<>();JSONObject apply=new JSONObject(request.toString()).put("operation","display_hud_on").put("graphs",new JSONArray().put("average_frame_rate")).put("stats",new JSONArray());
        JSONObject result=DisplayHudController.apply(apply,options->{calls.add(options.getString("operation"));return new JSONObject().put("accepted",true);});
        check(result.getBoolean("accepted")&&calls.equals(Arrays.asList("graph_hud_start","overlay_meta_on")),"One Start applies both renderers");
        JSONObject textOnlyApply=new JSONObject(textRequest.toString()).put("operation","display_hud_on").put("graphs",new JSONArray()).put("stats",new JSONArray());calls.clear();
        result=DisplayHudController.apply(textOnlyApply,options->{calls.add(options.getString("operation"));return new JSONObject().put("active",true).put("warning","HUD armed; waiting for game");});
        check(result.getBoolean("accepted")&&calls.equals(Arrays.asList("graph_hud_start","overlay_meta_on"))&&result.getString("warning").contains("armed"),"Text-only custom metrics keep the actual Meta HUD enabled");
        final boolean[] flags={false};DisplayHudController.apply(textOnlyApply,options->{if(options.getString("operation").equals("overlay_meta_on"))flags[0]=!options.getBoolean("custom_graphs_present")&&options.getBoolean("custom_values_present");return new JSONObject().put("accepted",true);});check(flags[0],"Custom text-only presence reaches Meta global visibility controls");
        GraphHudController raw=new GraphHudController();raw.start(textRequest);MetaMetricModel meta=new MetaMetricModel();
        JSONObject definition=meta.definitions(raw.status(now)).getJSONArray("Metrics").getJSONObject(0);
        check(!definition.getBoolean("ShowGraph")&&definition.getBoolean("ShowStat")&&definition.getString("DisplayName").contains("N/A"),"Unavailable text is registered honestly in the same HUD");
        check(meta.values(raw.status(now)).length()==0,"No invented zero submitted to Meta for a missing source");
        raw.sample(new JSONObject().put("name","Vulkan Known Draw Calls/s").put("observed_mono_ns",Long.toString(now)).put("value",12000));
        definition=meta.definitions(raw.status(now)).getJSONArray("Metrics").getJSONObject(0);
        check(definition.getDouble("GraphMax")>=12000&&!definition.getString("DisplayName").contains("N/A"),"Range and availability follow a real observed metric");
        check(meta.values(raw.status(now)).getDouble(MetaMetricModel.id("Vulkan Known Draw Calls/s"))==12000,"Measured values reach the public Meta JSON schema");
        check(meta.values(raw.status(now+4000000000L)).length()==0&&meta.definitions(raw.status(now+4000000000L)).getJSONArray("Metrics").getJSONObject(0).getString("DisplayName").contains("N/A"),"Stale values stop publishing and are marked unavailable");
        check(!MetaMetricModel.id("Vulkan Known Draw Calls/s").equals(MetaMetricModel.id("Vulkan Known Input Triangles/s")),"Stable distinct metric IDs");
        JSONObject decimal=new JSONObject().put("metrics",new JSONArray().put(new JSONObject().put("name","fraction").put("title","Triangles").put("unit","M triangles/s").put("graph_max",10).put("graph",true).put("text",true).put("status","measured").put("value",1.519)));
        JSONObject integers=meta.values(decimal);check(integers.get(MetaMetricModel.id("fraction")) instanceof Integer&&integers.getInt(MetaMetricModel.id("fraction"))==1519,"Meta integer-only service receives integral values with useful geometry precision");
        check(meta.definitions(decimal).getJSONArray("Metrics").getJSONObject(0).getString("DisplayName").contains("K triangles/s"),"HUD unit matches integer geometry representation");
        decimal.getJSONArray("metrics").getJSONObject(0).put("value",Double.MAX_VALUE);check(meta.values(decimal).length()==0,"Integer overflow is unavailable, never clamped to an invented measurement");
        calls.clear();try{DisplayHudController.apply(apply,options->{String operation=options.getString("operation");calls.add(operation);if(operation.equals("overlay_meta_on"))throw new IOException("Metric unavailable");return new JSONObject().put("active",true);});throw new AssertionError("Definition acceptance was treated as visible HUD success");}catch(IllegalArgumentException expected){check(calls.equals(Arrays.asList("graph_hud_start","overlay_meta_on","graph_hud_stop")),"Global HUD failure rolls back custom definitions and is reported as failure");}
        try{DisplayHudController.apply(apply,options->{throw new IOException("offline");});throw new AssertionError("Failed HUD reported success");}catch(IllegalArgumentException expected){++checks;}
        calls.clear();DisplayHudController.apply(new JSONObject().put("operation","display_hud_off"),options->{calls.add(options.getString("operation"));return new JSONObject();});
        check(calls.equals(Arrays.asList("graph_hud_stop","overlay_meta_off")),"One Stop hides both renderers");
        java.lang.reflect.Method select=MetaHudController.class.getDeclaredMethod("selection",JSONArray.class,int.class);select.setAccessible(true);JSONArray all=new JSONArray();for(String[] item:MetaHudController.METRICS)all.put(item[0]);
        check(((Set<?>)select.invoke(null,all,MetaHudController.METRICS.length)).size()==MetaHudController.METRICS.length,"All catalog HUD metrics selectable without the former six-graph cap");
        if(args.length>0){graphs.start(request);graphs.sample(new JSONObject().put("name","Vulkan Known Draw Calls/s").put("observed_mono_ns",Long.toString(now)).put("value",1250));try(FileOutputStream output=new FileOutputStream(args[0])){output.write(graphs.packet("org.cyinsights.gputest",now));}}
        System.out.println(checks+" HUD routing, provenance, gap, lifecycle and packet checks passed (synthetic fixtures)");
    }
}

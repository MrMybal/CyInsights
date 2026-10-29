// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;

import java.util.regex.*;
import java.util.UUID;
import org.json.*;

/** Documented CLI transport. Unknown geometry stays unavailable, never invented. */
public final class ProfilerParser {
    private static final Pattern PROCESS=Pattern.compile("^Process\\s+(\\S+)\\s+has\\s+.*",Pattern.CASE_INSENSITIVE);
    private static final Pattern SURFACE=Pattern.compile("^Surface\\s+(\\d+)\\s*\\|\\s*(\\d+)x(\\d+)\\s*\\|.*?MSAA\\s*(?:=\\s*)?(\\d+).*?\\|\\s*(\\d+)\\s+(\\d+)x(\\d+)\\s+bins.*?\\|\\s*([0-9.eE+-]+)\\s*ms.*",Pattern.CASE_INSENSITIVE);
    private static final Pattern BIN=Pattern.compile("^Bin\\s+(\\d+)\\s*\\|\\s*topLeft\\s+(\\d+)\\s*x\\s*(\\d+)\\s*\\|\\s*(\\d+)x(\\d+)\\s+logical\\s+bins.*",Pattern.CASE_INSENSITIVE);
    private static final Pattern STAGE=Pattern.compile("^Stage\\s+\\d+\\s*:\\s*([A-Za-z][A-Za-z0-9_]*)\\s*:?\\s+([0-9.eE+-]+)\\s*(ms|us|ns)\\b.*",Pattern.CASE_INSENSITIVE);
    public static JSONObject capture(String text,String target,String boot,String observed,boolean complete) throws JSONException {
        JSONObject out=new JSONObject();out.put("schema","cyinsightovr.capture");out.put("version",1);out.put("source","meta.ovrgpuprofiler");
        out.put("capture_id",UUID.randomUUID().toString());out.put("target_package",target);out.put("android_boot_id",boot);
        out.put("observed_mono_ns",observed);out.put("time_basis","capture_relative_unanchored");
        out.put("execution_frame",JSONObject.NULL);out.put("eye_identity",JSONObject.NULL);out.put("live_overlay",false);
        JSONArray surfaces=new JSONArray();out.put("surfaces",surfaces);
        String process="";JSONObject current=null,tile=null;int ordinal=0;java.util.LinkedHashSet<String> processes=new java.util.LinkedHashSet<>();boolean targetPresent=false;
        for(String original:text.split("[\\r\\n]+")) {
            String line=original.trim();Matcher m=PROCESS.matcher(line);
            if(m.matches()){process=m.group(1);if(process.equals(target))targetPresent=true;if(processes.size()<16)processes.add(process);current=null;tile=null;continue;}
            m=SURFACE.matcher(line);
            if(m.matches()) {
                tile=null;current=null;if(!process.equals(target))continue;
                int width=Integer.parseInt(m.group(2)),height=Integer.parseInt(m.group(3));double ms=Double.parseDouble(m.group(8));
                if(width<=0||height<=0||width>65536||height>65536||!Double.isFinite(ms)||ms<0)continue;
                current=new JSONObject();current.put("id",m.group(1)+"/"+(++ordinal));current.put("source_surface_index",m.group(1));current.put("process",process);
                current.put("width",width);current.put("height",height);current.put("msaa",Integer.parseInt(m.group(4)));
                current.put("tile_width",Integer.parseInt(m.group(6)));current.put("tile_height",Integer.parseInt(m.group(7)));
                current.put("duration_us",ms*1000);current.put("tiles",new JSONArray());surfaces.put(current);continue;
            }
            m=BIN.matcher(line);
            if(m.matches()) {
                tile=null;if(current==null)continue;
                int x=Integer.parseInt(m.group(2)),y=Integer.parseInt(m.group(3));
                long width=(long)current.getInt("tile_width")*Integer.parseInt(m.group(4)),height=(long)current.getInt("tile_height")*Integer.parseInt(m.group(5));
                if(x>=current.getInt("width")||y>=current.getInt("height")||width<=0||height<=0)continue;
                tile=new JSONObject();tile.put("id",Integer.parseInt(m.group(1)));tile.put("x",x);tile.put("y",y);
                tile.put("width",Math.min(width,current.getInt("width")-x));tile.put("height",Math.min(height,current.getInt("height")-y));
                tile.put("stages_us",new JSONObject());tile.put("source_bin_description",line);current.getJSONArray("tiles").put(tile);continue;
            }
            m=STAGE.matcher(line);
            if(m.matches()&&tile!=null) {
                double value=Double.parseDouble(m.group(2));if(!Double.isFinite(value)||value<0)continue;
                value*=m.group(3).equalsIgnoreCase("ms")?1000:m.group(3).equalsIgnoreCase("ns")?.001:1;
                JSONObject stages=tile.getJSONObject("stages_us");String key=m.group(1);stages.put(key,stages.optDouble(key,0)+value);
            }
        }
        int tiles=0,withoutGeometry=0;JSONArray measuredSurfaces=new JSONArray();
        for(int i=0;i<surfaces.length();++i){JSONObject s=surfaces.getJSONObject(i);int count=s.getJSONArray("tiles").length();
            if(count==0)++withoutGeometry;else{measuredSurfaces.put(s);tiles+=count;}}
        boolean measured=complete&&tiles>0;
        out.put("status",measured?"measured":"unavailable");
        String reason=measured?"Measured per-bin stage durations. Capture-relative snapshot; no per-eye or Unreal-frame assignment.":!complete?"Profiler output was incomplete, timed out or exceeded the bounded response size.":
            text.contains("No render stage data")?"The profiler captured no render stages. Keep the headset awake and the target rendering; detailed profiling must be active before its GPU context is created.":
            withoutGeometry>0?"The target reported surfaces without usable tile geometry. The source may lack detailed bin data; no heatmap was invented.":
            !targetPresent&&!processes.isEmpty()?"The profiler returned other processes but no surfaces for "+target+". The target is not attached to this detailed trace; game visibility alone does not prove profiling is active for its GPU context.":
            "No tile geometry for the selected package. Check the target, enable detailed profiling before starting it, and keep the game rendering.";
        out.put("reason",reason);out.put("profiler_output_complete",complete);out.put("target_process_present",targetPresent);out.put("observed_processes",new JSONArray(processes));out.put("source_target_surfaces",surfaces.length());out.put("surfaces_without_tile_geometry",withoutGeometry);
        out.put("surfaces",measured?measuredSurfaces:new JSONArray());
        out.put("tile_count",measured?tiles:0);return out;
    }
    public static JSONObject metric(String line,long mono) throws JSONException {
        int colon=line.lastIndexOf(':');if(colon<1)return null;String name=line.substring(0,colon).trim(),value=line.substring(colon+1).trim();
        if(name.isEmpty()||name.length()>180)return null;
        try {
            double n=Double.parseDouble(value);JSONObject result=new JSONObject();result.put("name",name);result.put("unit","raw");
            result.put("value",Double.isFinite(n)&&n>=0?n:JSONObject.NULL);result.put("observed_mono_ns",Long.toString(mono));result.put("scope","system");return result;
        }catch(NumberFormatException e){return null;}
    }
}

// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import org.json.*;
/** One selection UI; each source uses the renderer capable of displaying it. */
final class DisplayHudController {
    interface Requester {JSONObject request(JSONObject options)throws Exception;}
    static JSONObject apply(JSONObject options,Requester transport)throws Exception {
        boolean start=options.optString("operation").equals("display_hud_on");
        JSONArray custom=options.optJSONArray("custom_graphs"),graphs=options.optJSONArray("graphs"),stats=options.optJSONArray("stats");
        boolean wantsCustom=start&&custom!=null&&custom.length()>0,wantsMeta=start&&(wantsCustom||graphs!=null&&graphs.length()>0||stats!=null&&stats.length()>0);
        if(start&&!wantsCustom&&!wantsMeta)throw new IllegalArgumentException("Choose at least one HUD graph or value");
        boolean accepted=false,customStarted=false;int completed=0;StringBuilder warning=new StringBuilder();JSONObject result=new JSONObject();
        boolean customGraphs=false,customValues=false;if(wantsCustom)for(int i=0;i<custom.length();++i){customGraphs|=custom.getJSONObject(i).optBoolean("graph",true);customValues|=custom.getJSONObject(i).optBoolean("value",true);}
        for(boolean own:new boolean[]{true,false}){
            boolean enabled=own?wantsCustom:wantsMeta;String operation=own?(enabled?"graph_hud_start":"graph_hud_stop"):(enabled?"overlay_meta_on":"overlay_meta_off");
            try{JSONObject reply=transport.request(new JSONObject(options.toString()).put("operation",operation).put("custom_graphs_present",customGraphs).put("custom_values_present",customValues));
                if(reply.has("error"))throw new IllegalArgumentException(reply.optString("error"));result.put(own?"curve_hud":"meta_hud",reply);++completed;
                if(own&&enabled)customStarted=true;
                if(enabled)accepted=true;if(!reply.optString("warning").isEmpty())warning.append(reply.optString("warning")).append(" ");
            }catch(Exception error){
                if(start&&enabled){if(customStarted)try{transport.request(new JSONObject().put("operation","graph_hud_stop"));}catch(Exception cleanup){error.addSuppressed(cleanup);}throw new IllegalArgumentException((own?"Custom HUD metrics: ":"Meta HUD: ")+error.getMessage(),error);}
                if(!start)warning.append(own?"CyInsights metrics: ":"Meta HUD: ").append(error.getMessage()).append(" ");}
        }
        if(start?!accepted:completed==0)throw new IllegalArgumentException(warning.toString().trim());
        return result.put("accepted",true).put("active",start).put("warning",warning.toString().trim());
    }
}

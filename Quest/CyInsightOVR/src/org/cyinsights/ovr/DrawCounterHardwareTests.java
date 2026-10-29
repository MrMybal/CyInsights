// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import java.io.File;
import java.util.*;
import org.json.*;

/** Explicit real-data test; requires the dedicated project already rendering. */
public final class DrawCounterHardwareTests {
    private static final File CONFIG=new File("/data/local/tmp/cyinsightovr-connection.json");
    private static JSONObject rpc(String operation)throws Exception{return BridgeClient.request(CONFIG,new JSONObject().put("operation",operation));}
    public static void main(String[] args)throws Exception{
        final String target="org.cyinsights.gputest";int pid=0;
        try{
            for(int hz:new int[]{1,5,10,20}){
                JSONObject accepted=BridgeClient.request(CONFIG,new JSONObject().put("operation","draw_counters_start").put("package",target).put("hz",hz));
                if(accepted.has("error"))throw new AssertionError(accepted.getString("error"));
                long deadline=System.nanoTime()+(hz==1?4200000000L:1800000000L);String after="0",session="";Set<String> stamps=new HashSet<>();ArrayList<Double> rates=new ArrayList<>();double draws=0,triangles=0;
                while(System.nanoTime()<deadline){
                    JSONObject snapshot=BridgeClient.request(CONFIG,new JSONObject().put("operation","snapshot").put("include_history",true).put("after_sequence",after).put("history_session",session));
                    session=snapshot.optString("history_session");JSONObject counters=snapshot.getJSONObject("draw_counters");
                    if("measured".equals(counters.optString("status"))){
                        int current=counters.getInt("pid");if(pid!=0&&current!=pid)throw new AssertionError("Target process restarted during the cadence test");pid=current;
                        String stamp=counters.getString("observed_mono_ns");if(stamps.add(stamp)){
                            if(!counters.getBoolean("draws_complete")||!counters.getBoolean("triangles_complete"))throw new AssertionError("Dedicated scene geometry unexpectedly incomplete");
                            rates.add(counters.getDouble("source_hz"));draws+=counters.getDouble("draws");triangles+=counters.getDouble("known_input_triangles");
                        }
                    }
                    JSONArray history=snapshot.getJSONArray("history");if(history.length()>0)after=history.getJSONObject(history.length()-1).getString("history_sequence");
                    Thread.sleep(25);
                }
                if(rates.isEmpty()||draws<=0||triangles<=0)throw new AssertionError("No fresh positive measured geometry at "+hz+" Hz");
                Collections.sort(rates);double median=rates.get(rates.size()/2);
                if(median<hz*.4||median>hz*1.3)throw new AssertionError("Measured cadence inconsistent with request: "+median+" / "+hz);
                System.out.println("Requested "+hz+" Hz: "+stamps.size()+" unique measured windows, median "+String.format(Locale.ROOT,"%.2f",median)+" Hz; same target PID, complete input geometry");
                rpc("draw_counters_stop");
            }
        }finally{rpc("draw_counters_stop");}
    }
}

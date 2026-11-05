// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.content.Context;
import org.json.*;

/** Loaded by our optional Development Vulkan layer, under the game's real UID.
 * Meta scopes application metrics to the foreground VR application. Publishing
 * them from the companion is accepted but does not show them over that game.
 * Called only by the layer's IPC worker, never a rendering callback. */
public final class TargetMetaMetrics {
    private final MetaAppMetrics client;private final String packageName;
    private String definition="",values="",error="";private long lastObservation,lastSubmit;private int generation,defined,submitted;
    public TargetMetaMetrics(Context target){client=new MetaAppMetrics(target);packageName=target.getPackageName();}
    public synchronized String update(String packet){
        try{
            JSONObject wire=new JSONObject(packet);if(!packageName.equals(wire.getString("target")))throw new IllegalArgumentException("HUD publication target does not match the running application");
            JSONObject next=wire.getJSONObject("definition"),measured=wire.getJSONObject("values");
            if(next.getJSONArray("Metrics").length()>GraphHudController.MAX_GRAPHS)throw new IllegalArgumentException("Too many HUD definitions");
            // Validate before passing data to a firmware parser that requires ints.
            java.util.Iterator<String> keys=measured.keys();while(keys.hasNext()){Object value=measured.get(keys.next());if(!(value instanceof Integer)&&!(value instanceof Long)||((Number)value).longValue()<0||((Number)value).longValue()>Integer.MAX_VALUE)throw new IllegalArgumentException("Invalid integer HUD value");}
            if(defined==0&&next.getJSONArray("Metrics").length()==0)return status();
            client.connect();if(generation!=client.generation()){generation=client.generation();definition="";values="";lastObservation=0;}
            String text=next.toString();if(!text.equals(definition)){if(!client.define(text))throw new IllegalStateException("Meta rejected the game's HUD definitions");definition=text;defined=next.getJSONArray("Metrics").length();values="";lastObservation=0;}
            String data=measured.toString();long observation=wire.optLong("observation");
            if(measured.length()>0&&(!data.equals(values)||observation>lastObservation)){if(!client.submit(data))throw new IllegalStateException("Meta rejected the game's HUD values");values=data;submitted=measured.length();lastSubmit=System.nanoTime();lastObservation=observation;}
            if(defined==0){client.close();submitted=0;lastSubmit=0;}
            error="";
        }catch(Exception failure){error=failure.getMessage()==null?failure.getClass().getSimpleName():failure.getMessage();}
        return status();
    }
    private String status(){try{return new JSONObject().put("publisher_package",packageName).put("defined_metrics",defined).put("last_submitted_metrics",submitted)
        .put("last_submit_age_ms",lastSubmit==0?JSONObject.NULL:(System.nanoTime()-lastSubmit)/1e6).put("error",error).toString();}catch(JSONException invalid){return "{}";}}
    public synchronized void close(){try{if(defined>0)client.define("{\"Metrics\":[]}");}catch(Exception ignored){}client.close();defined=submitted=0;}
}

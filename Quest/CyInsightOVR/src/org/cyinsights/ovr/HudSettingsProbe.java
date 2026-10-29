// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.app.*;
import android.os.*;
import org.json.*;
/** Real service settings check; never claims headset visibility. Opens no Activity or game. */
public final class HudSettingsProbe extends Instrumentation {
    @Override public void onCreate(Bundle args){super.onCreate(args);start();}
    @Override public void onStart(){Bundle out=new Bundle();boolean started=false;
        try{
            JSONObject initial=BridgeClient.request(getTargetContext(),new JSONObject().put("operation","snapshot"));
            if(initial.optBoolean("meta_overlay_requested"))throw new Exception("Stop the current managed HUD before this test");
            JSONObject catalog=BridgeClient.request(getTargetContext(),new JSONObject().put("operation","hud_catalog"));
            if(catalog.has("error"))throw new Exception(catalog.optString("error"));
            int available=0;JSONArray metrics=catalog.getJSONArray("metrics");for(int i=0;i<metrics.length();++i)if(metrics.getJSONObject(i).optBoolean("available"))++available;
            out.putInt("available_hud_metrics",available);
            started=true;
            JSONObject applied=BridgeClient.request(getTargetContext(),new JSONObject().put("operation","overlay_meta_on").put("corner","center")
                .put("graphs",new JSONArray().put("average_frame_rate")).put("stats",new JSONArray().put("app_gpu_time_microseconds")));
            if(!applied.optBoolean("settings_verified"))throw new Exception("Real HUD settings failed: "+applied.optString("error"));
            out.putBoolean("selected_graph_and_value_verified",true);
            out.putBoolean("center_settings_verified","center".equals(applied.optString("corner")));
        }catch(Exception e){out.putString("error",e.toString());}
        finally{if(started)try{JSONObject restored=BridgeClient.request(getTargetContext(),new JSONObject().put("operation","overlay_meta_off"));if(!restored.optBoolean("settings_restored"))throw new Exception(restored.optString("error"));out.putBoolean("previous_settings_restored",true);}catch(Exception e){out.putString("error",e.toString());}}
        out.putString("visibility","not tested by this settings probe");finish(out.containsKey("error")?Activity.RESULT_CANCELED:Activity.RESULT_OK,out);
    }
}

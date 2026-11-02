// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.app.*;
import android.content.*;
import android.os.*;
import org.json.*;
import java.io.File;

/** Real APK-UID HUD control while the shell collector is absent. */
public final class StandaloneHudProbe extends Instrumentation {
    private JSONObject settings(Context c)throws Exception{
        Bundle result=c.getContentResolver().call(android.net.Uri.parse("content://com.oculus.ovrmonitormetricsservice.provider"),"QUERY_SERVICE_CONFIG",null,null);
        if(result==null||!result.getBoolean("Success"))throw new Exception("Cannot verify actual Meta settings");return new JSONObject(result.getString("Config"));
    }
    @Override public void onCreate(Bundle args){super.onCreate(args);start();}
    @Override public void onStart(){Bundle out=new Bundle();boolean started=false;JSONObject original=null;
        try{
            Context c=getTargetContext();if(android.os.Process.myUid()==2000)throw new Exception("Test must run as the APK, not shell");
            boolean collector=false;try{collector=BridgeClient.request(new File(c.getFilesDir(),"developer-bridge.json"),new JSONObject().put("operation","snapshot")).optBoolean("bridge");}catch(BridgeClient.Unavailable expected){}
            if(collector)throw new Exception("Stop the collector before this standalone test");
            if(!DirectMetaHud.available(c))throw new Exception("Standalone HUD authorization missing");
            original=settings(c);
            JSONObject snapshot=BridgeClient.request(c,new JSONObject().put("operation","snapshot"));
            if(!snapshot.optBoolean("standalone_hud")||snapshot.optBoolean("bridge"))throw new Exception("Standalone snapshot used a collector");
            JSONObject catalog=BridgeClient.request(c,new JSONObject().put("operation","hud_catalog"));int count=0;
            JSONArray metrics=catalog.getJSONArray("metrics");for(int i=0;i<metrics.length();++i)if(metrics.getJSONObject(i).optBoolean("available"))++count;
            out.putInt("available_hud_metrics",count);if(count==0)throw new Exception("No HUD metrics found");
            started=true;JSONObject applied=BridgeClient.request(c,new JSONObject().put("operation","overlay_meta_on").put("size","small").put("corner","upper_center").put("graphs",new JSONArray().put("average_frame_rate").put("app_gpu_time_microseconds")).put("stats",new JSONArray()));
            if(!applied.optBoolean("settings_verified")||!applied.optBoolean("selection_complete")||!applied.optBoolean("standalone_hud")||applied.optBoolean("collector_required",true))throw new Exception("Direct HUD settings not verified");
            out.putBoolean("direct_hud_start_verified",true);out.putBoolean("collector_absent",true);out.putBoolean("runs_as_application",true);
            JSONObject saved=new JSONObject(c.getSharedPreferences("MainActivity",0).getString("display_settings","{}"));
            saved.put("operation","overlay_meta_on");JSONObject configured=BridgeClient.request(c,saved);
            if(!configured.optBoolean("settings_verified"))throw new Exception("Saved HUD selection did not apply");
            int unavailable=configured.getJSONArray("unavailable_graphs").length()+configured.getJSONArray("unavailable_stats").length();
            if(unavailable>0&&(configured.optBoolean("selection_complete")||configured.optString("warning").isEmpty()))throw new Exception("Unavailable metrics were not reported");
            out.putBoolean("saved_selection_applied",true);out.putInt("saved_selection_unavailable_metrics",unavailable);
            out.putBoolean("read_logs_permission",c.checkSelfPermission("android.permission.READ_LOGS")==android.content.pm.PackageManager.PERMISSION_GRANTED);
            out.putString("result","passed");
        }catch(Exception e){out.putString("error",e.toString());}
        finally{if(started)try{JSONObject restored=BridgeClient.request(getTargetContext(),new JSONObject().put("operation","overlay_meta_off"));if(!restored.optBoolean("settings_restored"))throw new Exception("HUD settings not restored");
            JSONObject observed=settings(getTargetContext());
            for(String field:new String[]{"overlayEnabled","statsEnabled","graphEnabled","overlayScale","overlayDistance","overlayPitch","overlayYaw","overlayHeadLocked","metrics"})
                if(!original.get(field).toString().equals(observed.get(field).toString()))throw new Exception("HUD restore mismatch: "+field);
            out.putBoolean("direct_stop_and_restore_verified",true);}catch(Exception e){out.putString("error",e.toString());}}
        out.putString("visibility","requires user confirmation");finish(out.containsKey("error")?Activity.RESULT_CANCELED:Activity.RESULT_OK,out);
    }
}

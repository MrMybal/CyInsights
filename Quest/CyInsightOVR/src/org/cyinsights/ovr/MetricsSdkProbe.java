// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;

import android.app.*;
import android.content.*;
import android.os.*;
import dalvik.system.DexClassLoader;
import java.io.*;
import java.lang.reflect.*;
import org.json.*;

/** Optional public-SDK experiment. No Meta SDK binary is bundled in this APK. */
public final class MetricsSdkProbe extends Instrumentation {
    private int seconds=35;
    @Override public void onCreate(Bundle args){super.onCreate(args);if(args!=null)try{seconds=Math.max(5,Math.min(90,Integer.parseInt(args.getString("seconds","35"))));}catch(NumberFormatException ignored){}start();}
    private static Object call(Class<?> api,String method,Context context)throws Exception{
        return api.getMethod(method,Context.class).invoke(null,context);
    }
    @Override public void onStart(){
        Bundle result=new Bundle();Class<?> api=null;Context context=getTargetContext();
        try{
            File jar=new File(context.getFilesDir(),"ovr-metrics-sdk-probe.jar");
            if(!jar.isFile()||jar.canWrite())throw new IOException("Install the optional SDK probe DEX read-only into this app's private files");
            ClassLoader loader=new DexClassLoader(jar.toString(),context.getCodeCacheDir().toString(),null,context.getClassLoader());
            api=Class.forName("com.oculus.metrics.OVRMetricsToolClient",true,loader);
            call(api,"bind",context);
            long deadline=SystemClock.elapsedRealtime()+5000;
            while(SystemClock.elapsedRealtime()<deadline&&!Boolean.TRUE.equals(call(api,"isServiceAvailable",context)))Thread.sleep(100);
            if(!Boolean.TRUE.equals(call(api,"isServiceAvailable",context)))throw new IOException("Official SDK service unavailable");
            result.putBoolean("official_sdk_connected",true);
            JSONObject hud=BridgeClient.request(context,new JSONObject().put("operation","overlay_meta_on").put("size","small").put("corner","upper_left"));
            if(!hud.optBoolean("accepted"))throw new IOException("Meta HUD unavailable: "+hud.optString("error"));
            int accepted=0,events=0;String sample="";
            for(int tick=0;tick<seconds;++tick){
                String text="<color=#00C8FF>CYINSIGHTS TEST</color>\nStandalone companion / "+tick+"\n<color=#55DDBC>---===----====---</color>";
                if(Boolean.TRUE.equals(api.getMethod("setOverlayDebugString",Context.class,String.class).invoke(null,context,text)))++accepted;
                Object event=call(api,"getLatestEventJson",context);if(event instanceof String&&!((String)event).isEmpty()){sample=(String)event;++events;}
                if(tick==2){Bundle progress=new Bundle();progress.putString("phase","custom_text_running");progress.putInt("accepted_updates",accepted);sendStatus(1,progress);}
                Thread.sleep(1000);
            }
            result.putInt("accepted_updates",accepted);result.putInt("events_received",events);
            if(!sample.isEmpty()){
                JSONObject data=new JSONObject(sample);result.putInt("event_fields",data.length());
                try(FileOutputStream file=new FileOutputStream(new File(context.getFilesDir(),"metrics-sdk-probe-event.json"))){file.write(sample.getBytes(java.nio.charset.StandardCharsets.UTF_8));}
            }
            result.putString("result","Public SDK calls completed; headset visibility requires user confirmation");
        }catch(Exception e){result.putString("error",e.toString());}
        finally{
            if(api!=null){try{api.getMethod("setOverlayDebugString",Context.class,String.class).invoke(null,context,"");}catch(Exception ignored){}
                try{call(api,"shutdown",context);}catch(Exception ignored){}}
            try{BridgeClient.request(context,new JSONObject().put("operation","overlay_meta_off"));}catch(Exception ignored){}
        }
        finish(result.containsKey("error")?Activity.RESULT_CANCELED:Activity.RESULT_OK,result);
    }
}

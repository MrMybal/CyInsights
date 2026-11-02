// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.app.*;
import android.content.*;
import android.os.*;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import org.json.*;

/** Own-UID service startup, authenticated SDK, rejection and live source checks. Opens no game. */
public final class AutonomousServiceProbe extends Instrumentation {
    private boolean lifecycle;
    @Override public void onCreate(Bundle args){super.onCreate(args);lifecycle=args!=null&&"true".equals(args.getString("lifecycle"));start();}
    @Override public void onStart(){Bundle result=new Bundle();try{
        Context c=getTargetContext();if(android.os.Process.myUid()==2000)throw new Exception("Test must run as the APK");
        JSONObject snapshot=BridgeClient.request(c,new JSONObject().put("operation","snapshot"));
        if(!snapshot.optBoolean("bridge")||!"application".equals(snapshot.optString("service_owner"))||snapshot.optBoolean("pc_bootstrap_required",true))throw new Exception("GPU runtime is not owned by the APK");
        JSONObject status=BridgeClient.request(c,new JSONObject().put("operation","status"));if(!status.optString("profiler_status").contains("profiling is"))throw new Exception("Own-UID profiler query failed");
        JSONObject config;try(BufferedReader reader=new BufferedReader(new FileReader(new File(c.getFilesDir(),"developer-bridge.json")))){config=new JSONObject(reader.readLine());}
        try(Socket socket=new Socket("127.0.0.1",config.getInt("port"))){socket.setSoTimeout(4000);socket.getOutputStream().write("{\"operation\":\"snapshot\"}\n".getBytes(StandardCharsets.UTF_8));
            JSONObject denied=new JSONObject(new BufferedReader(new InputStreamReader(socket.getInputStream(),StandardCharsets.UTF_8)).readLine());if(!"Unauthorized bridge request".equals(denied.optString("error"))||denied.has("metrics"))throw new Exception("Unauthenticated caller was not rejected");}
        JSONObject denied=BridgeClient.request(c,new JSONObject().put("operation","arbitrary_exec"));if(!denied.has("error"))throw new Exception("Unexpected operation accepted");
        JSONObject caps=BridgeClient.request(c,new JSONObject().put("operation","companion_capabilities"));if(!"application".equals(caps.optString("service_owner"))||!"app_external".equals(caps.optString("storage")))throw new Exception("Desktop SDK does not use the own-app runtime");
        if(lifecycle){
            for(int cycle=0;cycle<2;++cycle){
                GpuDataService.stop(c);long until=System.nanoTime()+6000000000L;while(GpuDataService.running()&&System.nanoTime()<until)Thread.sleep(50);
                JSONObject stopped=BridgeClient.request(c,new JSONObject().put("operation","snapshot"));
                if(GpuDataService.running()||!stopped.optBoolean("gpu_service_stopped"))throw new Exception("The Stop control restarted its own GPU service");
                GpuDataService.require(c);JSONObject restarted=BridgeClient.request(c,new JSONObject().put("operation","snapshot"));
                if(!restarted.optBoolean("bridge")||!"application".equals(restarted.optString("service_owner")))throw new Exception("GPU service did not restart inside the APK");
            }
            result.putInt("stop_start_cycles",2);
        }
        Thread.sleep(1500);snapshot=BridgeClient.request(c,new JSONObject().put("operation","snapshot"));
        result.putBoolean("service_started_by_apk",true);result.putBoolean("runs_as_application",true);result.putBoolean("unauthenticated_rejected",true);result.putBoolean("arbitrary_command_rejected",true);result.putBoolean("desktop_protocol_available",true);result.putInt("observed_metrics",snapshot.getJSONArray("metrics").length());result.putString("result","passed");
        finish(Activity.RESULT_OK,result);
    }catch(Exception error){result.putString("error",error.toString());finish(Activity.RESULT_CANCELED,result);}}
}

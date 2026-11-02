// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.content.*;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.*;
import org.json.*;
import java.io.*;
import java.util.concurrent.*;

/** App-owned HUD control. No shell process, collector socket or log access. */
final class DirectMetaHud {
    static final String PERMISSION="android.permission.WRITE_SECURE_SETTINGS";
    private static final String PACKAGE="com.oculus.ovrmonitormetricsservice";
    private static final Uri PROVIDER=Uri.parse("content://"+PACKAGE+".provider");
    static final class SetupRequired extends IOException {
        SetupRequired(){super("Standalone HUD needs a one-time authorization. Connect the Quest to a PC and run Start-CyInsightOVR.ps1 -HudOnly -Launch from the complete Quest bundle. Afterwards, Start HUD works directly on the headset, including after reboot. Open the app to configure and start the HUD.");}
    }
    private static DirectMetaHud instance;
    private final Context context;private final MetaHudController controller;
    private DirectMetaHud(Context context){
        this.context=context.getApplicationContext();
        controller=new MetaHudController(this::run,new File(this.context.getFilesDir(),"meta-hud-original.json"),this::read);
    }
    private static synchronized DirectMetaHud get(Context c){if(instance==null)instance=new DirectMetaHud(c);return instance;}
    static boolean available(Context c){return c.checkSelfPermission(PERMISSION)==PackageManager.PERMISSION_GRANTED&&c.getPackageManager().resolveContentProvider(PACKAGE+".provider",0)!=null;}
    static boolean handles(String operation){return operation.equals("overlay_meta_on")||operation.equals("overlay_meta_off")||operation.equals("hud_catalog");}
    static JSONObject request(Context c,JSONObject request)throws Exception{
        if(!available(c))throw new SetupRequired();DirectMetaHud direct=get(c);String op=request.optString("operation");JSONObject result;
        if(op.equals("hud_catalog"))result=direct.controller.catalog();
        else if(op.equals("overlay_meta_on"))result=direct.controller.enable(request);
        else if(op.equals("overlay_meta_off"))result=direct.controller.disable();
        else throw new IllegalArgumentException("Unsupported standalone HUD operation");
        return result.put("standalone_hud",true).put("backend","meta.direct").put("collector_required",false);
    }
    static JSONObject snapshot(Context c)throws Exception{
        if(!available(c))throw new SetupRequired();DirectMetaHud direct=get(c);JSONObject observed=direct.read();
        return new JSONObject().put("bridge",false).put("standalone_hud",true).put("hud_backend","meta.direct")
            .put("status","Meta HUD ready. Start the on-device GPU service in Connection for GPU curves and the tile heatmap.")
            .put("vr_hud",direct.controller.state()).put("meta_overlay_enabled",observed.getBoolean("overlayEnabled"))
            .put("metrics",new JSONArray()).put("history",new JSONArray());
    }
    private JSONObject read()throws Exception{
        Bundle result=context.getContentResolver().call(PROVIDER,"QUERY_SERVICE_CONFIG",null,null);
        if(result==null||!result.getBoolean("Success")||result.getString("Config")==null)throw new IOException("This Meta firmware does not provide the standalone HUD settings interface");
        String text=result.getString("Config");if(text.length()>128*1024)throw new IOException("Invalid Meta HUD settings response");
        JSONObject state=new JSONObject(text);
        for(String field:new String[]{"overlayEnabled","overlayScale","overlayDistance","statsEnabled","graphEnabled","overlayPitch","overlayYaw","overlayHeadLocked","metrics"})
            if(!state.has(field))throw new IOException("Unsupported Meta HUD settings schema");
        return state;
    }
    private String run(String[] args,int timeout)throws Exception{
        if(args.length==3&&args[0].equals("/system/bin/pm")&&args[1].equals("path")&&args[2].equals(PACKAGE)){
            context.getPackageManager().getApplicationInfo(PACKAGE,0);return "package:installed";
        }
        if(args.length<6||!args[0].equals("/system/bin/am")||!args[1].equals("broadcast")||!args[2].equals("-n")||!args[3].equals(PACKAGE+"/.SettingsBroadcastReceiver")||!args[4].equals("-a")||!args[5].startsWith(PACKAGE+"."))throw new IOException("Unsupported direct HUD command");
        Intent intent=new Intent(args[5]).setComponent(new ComponentName(PACKAGE,PACKAGE+".SettingsBroadcastReceiver"));
        for(int i=6;i<args.length;i+=3){if(i+2>=args.length)throw new IOException("Invalid HUD broadcast extras");
            if(args[i].equals("--es"))intent.putExtra(args[i+1],args[i+2]);
            else if(args[i].equals("--ez"))intent.putExtra(args[i+1],Boolean.parseBoolean(args[i+2]));
            else if(args[i].equals("--ef"))intent.putExtra(args[i+1],Float.parseFloat(args[i+2]));
            else throw new IOException("Unsupported HUD broadcast extra");
        }
        CountDownLatch done=new CountDownLatch(1);
        context.sendOrderedBroadcast(intent,null,new BroadcastReceiver(){@Override public void onReceive(Context c,Intent i){done.countDown();}},new Handler(Looper.getMainLooper()),0,null,null);
        if(!done.await(timeout,TimeUnit.SECONDS))throw new IOException("Meta HUD control timed out");
        return "Broadcast delivered";
    }
}

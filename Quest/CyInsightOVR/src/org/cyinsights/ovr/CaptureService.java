// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;

import android.app.*;
import android.content.*;
import android.os.*;
import org.json.*;
import java.io.*;
import java.util.concurrent.*;

/** Capture after the configuration panel closes, without taking VR focus. */
public final class CaptureService extends Service {
    private static final int NOTIFICATION=3;
    private final ExecutorService worker=Executors.newSingleThreadExecutor();
    private final Handler ui=new Handler(Looper.getMainLooper());
    private volatile boolean cancelled=false;
    private volatile String lastMessage="";
    public static volatile boolean running=false;
    public static volatile String phase="idle",completedId="",failure="";
    private Notification notification(String text,boolean canCancel){
        PendingIntent open=PendingIntent.getActivity(this,30,new Intent(this,MainActivity.class).putExtra("open_capture",true),PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE);
        Notification.Builder builder=new Notification.Builder(this,"cyovr-capture")
            .setSmallIcon(android.R.drawable.ic_menu_camera).setContentTitle("CyInsightOVR GPU snapshot")
            .setContentText(text).setStyle(new Notification.BigTextStyle().bigText(text)).setContentIntent(open).setOngoing(running);
        if(canCancel){PendingIntent cancel=PendingIntent.getService(this,31,new Intent(this,CaptureService.class).setAction("cancel"),PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE);
            builder.addAction(new Notification.Action.Builder(null,"Cancel scheduled capture",cancel).build());}
        return builder.build();
    }
    private void update(String text,boolean canCancel){lastMessage=text;((NotificationManager)getSystemService(NOTIFICATION_SERVICE)).notify(NOTIFICATION,notification(text,canCancel));}
    @Override public void onCreate(){super.onCreate();NotificationManager manager=(NotificationManager)getSystemService(NOTIFICATION_SERVICE);
        manager.createNotificationChannel(new NotificationChannel("cyovr-capture","CyInsightOVR GPU captures",NotificationManager.IMPORTANCE_LOW));}
    @Override public int onStartCommand(Intent intent,int flags,int startId){
        if(intent!=null&&"cancel".equals(intent.getAction())){
            synchronized(this){if("countdown".equals(phase)){cancelled=true;phase="cancelled";lastMessage="Scheduled capture cancelled";stopSelf();}}return START_NOT_STICKY;
        }
        if(running)return START_NOT_STICKY;
        String target=intent==null?"":intent.getStringExtra("package");
        if(target==null||!target.matches("[A-Za-z][A-Za-z0-9_]*(?:\\.[A-Za-z0-9_]+)+")){stopSelf();return START_NOT_STICKY;}
        running=true;cancelled=false;completedId="";failure="";phase="countdown";
        startForeground(NOTIFICATION,notification("Starts in 5 seconds. Return to the selected game and keep it rendering.",true));
        worker.execute(()->record(target,startId));return START_NOT_STICKY;
    }
    private void record(String target,int startId){
        try{
            for(int left=5;left>0&&!cancelled;--left){update("Capturing "+target+" in "+left+" s. Keep the game visible.",true);Thread.sleep(1000);}
            synchronized(this){if(cancelled)return;phase="capturing";}
            update("Recording a short GPU snapshot of "+target+"...",false);
            JSONObject accepted=BridgeClient.request(this,new JSONObject().put("operation","capture").put("package",target).put("seconds",.3));
            if(accepted.has("error"))throw new IOException(accepted.optString("error"));
            String job=accepted.optString("capture_id");
            if(!accepted.optBoolean("accepted")||job.isEmpty())throw new IOException("Collector must be updated: no capture job identity received");
            long deadline=SystemClock.elapsedRealtime()+30000;
            while(!cancelled&&SystemClock.elapsedRealtime()<deadline){
                JSONObject state=BridgeClient.request(this,new JSONObject().put("operation","snapshot"));
                if(state.has("error"))throw new IOException(state.optString("error"));
                JSONObject result=state.optJSONObject("capture");
                if(result!=null&&job.equals(result.optString("capture_id"))){
                    if(!target.equals(result.optString("target_package")))throw new IOException("Capture target mismatch");
                    CaptureStore.save(this,result);completedId=job;phase=result.optString("status","unavailable");
                    update("measured".equals(phase)?"Saved "+result.optInt("tile_count")+" measured tiles. Open CyInsightOVR to inspect the heatmap.":"Saved diagnostic: "+result.optString("reason"),false);
                    return;
                }
                Thread.sleep(500);
            }
            if(!cancelled)throw new IOException("Collector did not complete this capture within 30 seconds");
        }catch(InterruptedException e){Thread.currentThread().interrupt();phase="cancelled";}
        catch(Exception e){phase="failed";failure=e.getMessage()==null?e.getClass().getSimpleName():e.getMessage();update("Capture failed: "+failure,false);}
        finally{
            ui.post(()->{
                running=false;update(lastMessage,false);
                // Keep the result notification. No panel is opened over the game.
                // Serialize completion with onStartCommand, including duplicate starts.
                stopForeground(STOP_FOREGROUND_DETACH);stopSelf();
            });
        }
    }
    @Override public IBinder onBind(Intent intent){return null;}
    @Override public void onDestroy(){cancelled=true;worker.shutdownNow();running=false;super.onDestroy();}
}

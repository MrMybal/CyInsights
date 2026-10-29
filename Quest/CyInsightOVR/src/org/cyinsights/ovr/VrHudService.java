// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.app.*;
import android.content.*;
import android.os.*;
import org.json.*;
import java.util.concurrent.*;

/** Keeps an explicit Stop control while the configuration panel is closed. */
public final class VrHudService extends Service {
    public static volatile boolean running=false;
    private final ExecutorService worker=Executors.newSingleThreadExecutor();
    @Override public void onCreate(){
        super.onCreate();NotificationManager manager=(NotificationManager)getSystemService(NOTIFICATION_SERVICE);
        manager.createNotificationChannel(new NotificationChannel("cyovr-vr-hud","CyInsightOVR VR HUD",NotificationManager.IMPORTANCE_LOW));
        PendingIntent open=PendingIntent.getActivity(this,3,new Intent(this,MainActivity.class),PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE);
        PendingIntent stop=PendingIntent.getService(this,4,new Intent(this,VrHudService.class).setAction("stop"),PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE);
        startForeground(2,new Notification.Builder(this,"cyovr-vr-hud").setSmallIcon(android.R.drawable.ic_menu_view)
            .setContentTitle("CyInsightOVR compact VR HUD").setContentText("Rendered by Meta. Open to configure, or Stop to restore previous HUD settings.")
            .setContentIntent(open).addAction(new Notification.Action.Builder(null,"Stop HUD",stop).build()).setOngoing(true).build());
        running=true;
    }
    @Override public int onStartCommand(Intent intent,int flags,int id){
        if(intent!=null&&"stop".equals(intent.getAction()))worker.execute(()->{
            try{JSONObject result=BridgeClient.request(this,new JSONObject().put("operation","overlay_meta_off"));
                if(result.has("error"))throw new IllegalStateException(result.optString("error"));
                stopSelf();
            }catch(Exception e){android.util.Log.e("CyInsightOVR","HUD stop failed; reopen CyInsightOVR and retry",e);}
        });
        return START_NOT_STICKY;
    }
    @Override public IBinder onBind(Intent intent){return null;}
    @Override public void onDestroy(){running=false;worker.shutdown();stopForeground(STOP_FOREGROUND_REMOVE);super.onDestroy();}
}

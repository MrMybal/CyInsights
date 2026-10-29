// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.app.*;import android.content.*;import android.os.*;import org.json.*;
/** Keeps a visible stop/configuration control while the shell collector streams. */
public final class LiveHeatmapService extends Service {
    @Override public void onCreate(){super.onCreate();NotificationManager manager=getSystemService(NotificationManager.class);manager.createNotificationChannel(new NotificationChannel("cyovr-live-tiles","CyInsightOVR live tile heatmap",NotificationManager.IMPORTANCE_LOW));
        PendingIntent open=PendingIntent.getActivity(this,42,QuickAccess.configurationIntent(this).putExtra("open_heatmap_settings",true),PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE);
        PendingIntent stop=PendingIntent.getService(this,43,new Intent(this,LiveHeatmapService.class).setAction("stop"),PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE);
        startForeground(4,new Notification.Builder(this,"cyovr-live-tiles").setSmallIcon(android.R.drawable.ic_menu_view).setContentTitle("CyInsightOVR tile heatmap")
            .setContentText("GPU tile sampler active. Tap to configure or stop.").setContentIntent(open).setOngoing(true).addAction(new Notification.Action.Builder(null,"Stop heatmap",stop).build()).build());
    }
    @Override public int onStartCommand(Intent intent,int flags,int id){if(intent!=null&&"stop".equals(intent.getAction()))new Thread(()->{try{BridgeClient.request(this,new JSONObject().put("operation","heatmap_stop"));}catch(Exception error){android.util.Log.e("CyInsightOVR","Heatmap stop failed",error);}new Handler(Looper.getMainLooper()).post(this::stopSelf);},"CyOVR-Stop-Heatmap").start();return START_NOT_STICKY;}
    @Override public IBinder onBind(Intent intent){return null;}
    @Override public void onDestroy(){stopForeground(STOP_FOREGROUND_REMOVE);super.onDestroy();}
}

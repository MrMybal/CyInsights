// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.app.*;
import android.content.*;
import android.graphics.*;
import android.os.*;
import android.provider.Settings;
import android.view.*;
import java.util.*;
import java.util.concurrent.*;
import org.json.*;

/** Non-interactive Android HUD. VR compositor visibility is separately validated. */
public final class OverlayService extends Service {
    private WindowManager wm;private Hud hud;private ScheduledExecutorService worker;
    private final Handler ui=new Handler(Looper.getMainLooper());
    public static volatile boolean windowAttached=false;
    @Override public void onCreate() {
        super.onCreate();if(!Settings.canDrawOverlays(this)){stopSelf();return;}
        NotificationManager notifications=(NotificationManager)getSystemService(NOTIFICATION_SERVICE);
        notifications.createNotificationChannel(new NotificationChannel("cyovr-overlay","CyInsightOVR developer HUD",NotificationManager.IMPORTANCE_LOW));
        PendingIntent stop=PendingIntent.getService(this,1,new Intent(this,OverlayService.class).setAction("stop"),PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE);
        PendingIntent open=PendingIntent.getActivity(this,2,new Intent(this,MainActivity.class),PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE);
        startForeground(1,new Notification.Builder(this,"cyovr-overlay").setSmallIcon(android.R.drawable.ic_menu_view).setContentTitle("CyInsightOVR HUD is running").setContentText("System GPU counters. Tap to configure; Stop removes the HUD.").setContentIntent(open).addAction(new Notification.Action.Builder(null,"Stop HUD",stop).build()).setOngoing(true).build());
        wm=(WindowManager)getSystemService(WINDOW_SERVICE);hud=new Hud(this);
        WindowManager.LayoutParams params=new WindowManager.LayoutParams(600,400,WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE|WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE|WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,PixelFormat.TRANSLUCENT);
        params.gravity=Gravity.TOP|Gravity.LEFT;params.x=35;params.y=70;params.alpha=.8f;params.setTitle("CyInsightOVR Developer HUD");
        try{wm.addView(hud,params);windowAttached=true;}catch(RuntimeException e){stopSelf();return;}
        worker=Executors.newSingleThreadScheduledExecutor();worker.scheduleWithFixedDelay(()->{
            try{JSONObject value=BridgeClient.request(this,new JSONObject().put("operation","snapshot"));ui.post(()->{if(hud!=null)hud.update(value);});}
            catch(Exception e){ui.post(()->{if(hud!=null){hud.status="Collector unavailable";hud.invalidate();}});}
        },0,1000,TimeUnit.MILLISECONDS);
    }
    @Override public int onStartCommand(Intent i,int flags,int id){if(i!=null&&"stop".equals(i.getAction()))stopSelf();return START_NOT_STICKY;}
    @Override public IBinder onBind(Intent i){return null;}
    @Override public void onDestroy(){if(worker!=null)worker.shutdownNow();if(wm!=null&&hud!=null){try{wm.removeView(hud);}catch(RuntimeException ignored){}}hud=null;windowAttached=false;stopForeground(STOP_FOREGROUND_REMOVE);super.onDestroy();}
    private static final class Hud extends View {
        private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);private final ArrayDeque<Float> samples=new ArrayDeque<>();
        private JSONObject data=new JSONObject();String status="Waiting for measured counters";
        Hud(Context context){super(context);setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);}
        void update(JSONObject value){data=value;status=value.optString("status");double busy=metric("GPU % Bus Busy");samples.addLast(Double.isFinite(busy)?(float)busy:Float.NaN);while(samples.size()>90)samples.removeFirst();invalidate();}
        double metric(String name){JSONArray a=data.optJSONArray("metrics");if(a==null)return Double.NaN;for(int i=0;i<a.length();++i){JSONObject m=a.optJSONObject(i);if(m==null||!name.equals(m.optString("name")))continue;try{long timestamp=Long.parseLong(m.optString("observed_mono_ns","0"));if(System.nanoTime()-timestamp>3000000000L)return Double.NaN;}catch(Exception e){return Double.NaN;}return m.optDouble("value",Double.NaN);}return Double.NaN;}
        void text(Canvas c,String s,float x,float y,int size,int color){paint.setColor(color);paint.setTextSize(size);c.drawText(s,x,y,paint);}
        String value(double v,String suffix){return Double.isFinite(v)?String.format(java.util.Locale.ROOT,"%.1f%s",v,suffix):"N/A";}
        @Override protected void onDraw(Canvas c){
            super.onDraw(c);paint.setColor(Color.rgb(13,22,34));c.drawRoundRect(0,0,getWidth(),getHeight(),18,18,paint);
            text(c,"CyInsightOVR / SYSTEM GPU",20,35,24,Color.CYAN);
            text(c,"Bus "+value(metric("GPU % Bus Busy"),"%")+"   GPU "+value(metric("GPU Frequency")/1e6," MHz"),20,72,23,Color.WHITE);
            text(c,"Fragments "+value(metric("% Time Shading Fragments"),"%")+"   Vertices "+value(metric("% Time Shading Vertices"),"%"),20,107,20,Color.WHITE);
            float step=550f/89;int index=0;float last=Float.NaN;paint.setColor(Color.CYAN);paint.setStrokeWidth(2);
            for(float v:samples){if(Float.isFinite(v)&&Float.isFinite(last))c.drawLine(20+(index-1)*step,210-last*.8f,20+index*step,210-v*.8f,paint);last=v;++index;}
            text(c,"90 s history | sampled counters, not frame times",20,244,17,Color.LTGRAY);
            JSONObject capture=data.optJSONObject("capture");
            if(capture!=null&&"measured".equals(capture.optString("status"))){
                JSONArray surfaces=capture.optJSONArray("surfaces");JSONObject surface=surfaces!=null?surfaces.optJSONObject(0):null;
                if(surface!=null){JSONArray tiles=surface.optJSONArray("tiles");double maximum=0;
                    for(int i=0;i<tiles.length();++i)maximum=Math.max(maximum,tiles.optJSONObject(i).optJSONObject("stages_us").optDouble("Render",0));
                    float sx=180f/surface.optInt("width",1),sy=110f/surface.optInt("height",1);
                    for(int i=0;i<tiles.length();++i){JSONObject t=tiles.optJSONObject(i);double cost=t.optJSONObject("stages_us").optDouble("Render",Double.NaN);float n=maximum>0?(float)(cost/maximum):0;
                        paint.setColor(Double.isFinite(cost)?Color.rgb((int)(50+205*n),(int)(200*(1-n)),65):Color.GRAY);
                        c.drawRect(20+t.optInt("x")*sx,270+t.optInt("y")*sy,20+(t.optInt("x")+t.optInt("width"))*sx,270+(t.optInt("y")+t.optInt("height"))*sy,paint);}
                    text(c,"LAST SNAPSHOT",220,294,18,Color.YELLOW);text(c,"Render per tile",220,321,18,Color.LTGRAY);text(c,"Not aligned to current image",220,348,16,Color.LTGRAY);
                }
            }else text(c,"No measured tile snapshot",20,298,19,Color.LTGRAY);
        }
    }
}

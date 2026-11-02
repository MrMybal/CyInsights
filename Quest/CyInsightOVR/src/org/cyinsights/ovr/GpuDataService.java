// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.app.*;
import android.content.*;
import android.os.*;
import java.io.IOException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.CountDownLatch;

/** The APK owns and starts its GPU runtime. No shell bootstrap or external app. */
public final class GpuDataService extends Service {
    private static final Object STATE=new Object();private static volatile GpuDataService instance;
    private static boolean launching;private static String failure="";
    private static volatile CountDownLatch previousShutdown=new CountDownLatch(0);
    private final ShellBridge runtime=new ShellBridge();private volatile boolean ready,closing;private Thread worker;
    static boolean enabled(Context c){return c.getSharedPreferences("gpu_service",0).getBoolean("enabled",true);}
    static boolean running(){GpuDataService service=instance;return service!=null&&service.ready&&!service.closing;}
    static void start(Context c){
        c.getSharedPreferences("gpu_service",0).edit().putBoolean("enabled",true).apply();
        synchronized(STATE){if(running()||launching)return;launching=true;failure="";}
        try{c.startForegroundService(new Intent(c,GpuDataService.class));}
        catch(RuntimeException error){synchronized(STATE){launching=false;failure=error.getMessage();STATE.notifyAll();}throw error;}
    }
    static void require(Context c)throws Exception{
        if(Looper.myLooper()==Looper.getMainLooper())throw new IllegalStateException("GPU startup wait must run off the UI thread");start(c);
        long until=System.nanoTime()+TimeUnit.SECONDS.toNanos(6);
        synchronized(STATE){while(!running()&&failure.isEmpty()){long wait=until-System.nanoTime();if(wait<=0)break;TimeUnit.NANOSECONDS.timedWait(STATE,wait);}
            if(!running())throw new IOException(failure.isEmpty()?"GPU service is still starting. Retry Start in a moment.":"GPU service could not start: "+failure);}
    }
    static ParcelFileDescriptor openLayerChannel(int uid,int pid)throws IOException{
        GpuDataService service=instance;if(service==null||!service.ready||service.closing)throw new IOException("Open CyInsightOVR to start its GPU service");
        return service.runtime.openLayerChannel(uid,pid);
    }
    static void stop(Context c){c.getSharedPreferences("gpu_service",0).edit().putBoolean("enabled",false).apply();c.stopService(new Intent(c,GpuDataService.class));}
    @Override public void onCreate(){super.onCreate();instance=this;
        NotificationManager manager=getSystemService(NotificationManager.class);manager.createNotificationChannel(new NotificationChannel("cyovr-gpu-service","CyInsightOVR GPU service",NotificationManager.IMPORTANCE_LOW));
        PendingIntent open=PendingIntent.getActivity(this,60,QuickAccess.configurationIntent(this),PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE);
        PendingIntent stop=PendingIntent.getService(this,61,new Intent(this,GpuDataService.class).setAction("stop"),PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE);
        startForeground(6,new Notification.Builder(this,"cyovr-gpu-service").setSmallIcon(android.R.drawable.ic_menu_view).setContentTitle("CyInsightOVR GPU service")
            .setContentText("Running on this Quest. Open to configure; no PC is required.").setContentIntent(open).setOngoing(true).addAction(new Notification.Action.Builder(null,"Stop GPU service",stop).build()).build());
        CountDownLatch previous=previousShutdown;
        worker=new Thread(()->{try{if(!previous.await(5,TimeUnit.SECONDS))throw new IOException("Previous GPU service is still stopping. Retry Start shortly.");runtime.runLocal(this,()->{synchronized(STATE){if(!closing){ready=true;launching=false;STATE.notifyAll();}}});}
            catch(Exception error){android.util.Log.e("CyInsightOVR","Local GPU runtime failed",error);synchronized(STATE){if(!closing){failure=error.getMessage()==null?error.getClass().getSimpleName():error.getMessage();launching=false;STATE.notifyAll();}}}
            finally{ready=false;runtime.stopLocal();if(!closing)stopSelf();}},"CyOVR-GPU-Service");worker.start();
    }
    @Override public int onStartCommand(Intent intent,int flags,int id){if(intent!=null&&"stop".equals(intent.getAction())){stop(this);return START_NOT_STICKY;}return START_STICKY;}
    @Override public IBinder onBind(Intent intent){return null;}
    @Override public void onDestroy(){closing=true;ready=false;synchronized(STATE){if(instance==this){instance=null;launching=false;}STATE.notifyAll();}
        CountDownLatch stopped=new CountDownLatch(1);previousShutdown=stopped;new Thread(()->{try{runtime.stopLocal();}finally{stopped.countDown();}},"CyOVR-GPU-Shutdown").start();stopForeground(STOP_FOREGROUND_REMOVE);super.onDestroy();}
}

// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.app.*;
import android.content.*;
import android.graphics.Bitmap;
import android.os.*;
import android.provider.Settings;
import android.view.PixelCopy;
import java.io.*;
import java.util.concurrent.*;
/** Development-only own-app checks. Capture mode opens only our dedicated GPU test project. */
public final class SmokeTest extends Instrumentation {
    private boolean testHud=false,testVrHud=false,testCapture=false;
    @Override public void onCreate(Bundle arguments){super.onCreate(arguments);testHud=arguments!=null&&"true".equals(arguments.getString("android_hud"));testVrHud=arguments!=null&&"true".equals(arguments.getString("meta_hud"));testCapture=arguments!=null&&"true".equals(arguments.getString("capture_flow"));start();}
    private void click(MainActivity main,boolean capture)throws Exception{
        float[] point=null;long renderDeadline=SystemClock.uptimeMillis()+8000;
        while(SystemClock.uptimeMillis()<renderDeadline){point=capture?main.captureButtonForTest():main.hudButtonForTest();if(point[0]>=0&&point[1]>=0)break;Thread.sleep(100);}
        if(point==null||point[0]<0)throw new IOException("Native button was not rendered in the Quest panel");
        final float x=point[0],y=point[1];
        runOnMainSync(()->{long now=SystemClock.uptimeMillis();android.view.MotionEvent e=android.view.MotionEvent.obtain(now,now,android.view.MotionEvent.ACTION_DOWN,x,y,0);main.renderViewForTest().dispatchTouchEvent(e);e.recycle();});
        Thread.sleep(120);
        runOnMainSync(()->{long now=SystemClock.uptimeMillis();android.view.MotionEvent e=android.view.MotionEvent.obtain(now,now,android.view.MotionEvent.ACTION_UP,x,y,0);main.renderViewForTest().dispatchTouchEvent(e);e.recycle();});
    }
    @Override public void onStart(){Bundle result=new Bundle();
        try {
            Context context=getTargetContext();Intent intent=new Intent(context,MainActivity.class).setAction(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            if(testCapture)context.getSharedPreferences("MainActivity",0).edit().putString("target","org.cyinsights.gputest").commit();
            Activity activity=startActivitySync(intent);waitForIdleSync();Thread.sleep(1800);
            MainActivity main=(MainActivity)activity;
            if(testCapture){
                String previousCompleted=CaptureService.completedId;
                click(main,true);
                long closeDeadline=SystemClock.elapsedRealtime()+5000;
                while(SystemClock.elapsedRealtime()<closeDeadline&&!main.isFinishing())Thread.sleep(100);
                if(!main.isFinishing())throw new IOException("GPU capture did not close the configuration panel");
                result.putBoolean("configuration_closed",true);
                // This fixed explicit component is our test project, never a user game.
                context.startActivity(new Intent().setClassName("org.cyinsights.gputest","com.epicgames.unreal.GameActivity").addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
                long deadline=SystemClock.elapsedRealtime()+40000;
                while(SystemClock.elapsedRealtime()<deadline&&CaptureService.failure.isEmpty()&&
                    (CaptureService.running||CaptureService.completedId.isEmpty()||CaptureService.completedId.equals(previousCompleted)))Thread.sleep(100);
                if(!CaptureService.failure.isEmpty())throw new IOException(CaptureService.failure);
                org.json.JSONObject saved=CaptureStore.latest(context);
                if(saved==null||!CaptureService.completedId.equals(saved.optString("capture_id")))throw new IOException("No newly completed capture persisted by the background service");
                if(!"org.cyinsights.gputest".equals(saved.optString("target_package")))throw new IOException("Wrong target in saved GPU capture");
                result.putString("capture_id",saved.optString("capture_id"));result.putInt("measured_tile_count",saved.optInt("tile_count"));result.putString("capture_status",saved.optString("status"));
                if(!"measured".equals(saved.optString("status"))||saved.optInt("tile_count")<=0)throw new IOException("Real GPU capture unavailable: "+saved.optString("reason"));
                // Return from our own immersive test before inspecting a panel,
                // just as the user opens configuration from the Quest menu.
                try(android.os.ParcelFileDescriptor home=getUiAutomation().executeShellCommand("input keyevent KEYCODE_HOME")){}
                Thread.sleep(600);
                activity=startActivitySync(intent);main=(MainActivity)activity;waitForIdleSync();Thread.sleep(2000);
                org.json.JSONObject parsed=main.captureReportForTest();
                if(!saved.optString("capture_id").equals(parsed.optString("capture_id"))||parsed.optInt("tiles")!=saved.optInt("tile_count")||!parsed.optString("parse_error").isEmpty())throw new IOException("Native heatmap reader did not load the saved real capture: "+parsed.toString());
                result.putInt("native_reader_tiles",parsed.optInt("tiles"));
                result.putBoolean("saved_capture_reopened",true);
            }
            if(testVrHud){
                try{
                    main.pageForTest(2);Thread.sleep(600);
                    final MainActivity hudPanel=main;
                    for(int scroll=0;scroll<4;++scroll){runOnMainSync(()->hudPanel.renderViewForTest().dispatchKeyEvent(new android.view.KeyEvent(android.view.KeyEvent.ACTION_DOWN,android.view.KeyEvent.KEYCODE_PAGE_DOWN)));Thread.sleep(150);}
                    click(main,false);
                    long deadline=SystemClock.uptimeMillis()+15000;
                    while(SystemClock.uptimeMillis()<deadline&&!VrHudService.running)Thread.sleep(100);
                    if(main.isFinishing()||!VrHudService.running)throw new IOException("HUD button closed configuration or did not retain stop controller");
                    org.json.JSONObject state=BridgeClient.request(context,new org.json.JSONObject().put("operation","snapshot"));
                    if(!state.getJSONObject("vr_hud").getBoolean("requested"))throw new IOException("VR HUD not active in bridge");
                    result.putBoolean("configuration_stayed_open",true);result.putBoolean("hud_controller_running",true);
                    result.putString("result","Selected Meta HUD requested from native button; configuration stayed open");
                }finally{
                    BridgeClient.request(context,new org.json.JSONObject().put("operation","overlay_meta_off"));
                    runOnMainSync(()->context.stopService(new Intent(context,VrHudService.class)));
                }
                finish(Activity.RESULT_OK,result);return;
            }
            org.json.JSONObject snapshot=BridgeClient.request(context,new org.json.JSONObject().put("operation","snapshot"));
            if(!snapshot.optBoolean("bridge"))throw new IOException("App-to-collector probe failed");
            result.putInt("measured_system_metric_count",snapshot.getJSONArray("metrics").length());
            result.putBoolean("overlay_permission",Settings.canDrawOverlays(context));
            if(testHud&&Settings.canDrawOverlays(context)){
                runOnMainSync(()->context.startForegroundService(new Intent(context,OverlayService.class)));
                Thread.sleep(800);result.putBoolean("android_hud_window_attached",OverlayService.windowAttached);
                if(!OverlayService.windowAttached)throw new IOException("Experimental Android HUD window did not attach");
            }
            CountDownLatch done=new CountDownLatch(1);final int[] code={-1};final Bitmap[] image={null};
            final MainActivity rendered=main;final Activity renderedActivity=activity;
            runOnMainSync(()->{image[0]=Bitmap.createBitmap(Math.max(1,rendered.renderViewForTest().getWidth()),Math.max(1,rendered.renderViewForTest().getHeight()),Bitmap.Config.ARGB_8888);
                try{PixelCopy.request(renderedActivity.getWindow(),image[0],value->{code[0]=value;done.countDown();},new Handler(Looper.getMainLooper()));}
                catch(IllegalArgumentException e){code[0]=PixelCopy.ERROR_SOURCE_INVALID;done.countDown();}});
            if(!done.await(6,TimeUnit.SECONDS)||code[0]!=PixelCopy.SUCCESS)throw new IOException("Own-window PixelCopy failed: "+code[0]);
            File output=new File(context.getExternalFilesDir(null),"own-app-smoke.png");try(FileOutputStream out=new FileOutputStream(output)){image[0].compress(Bitmap.CompressFormat.PNG,100,out);}
            result.putString("screenshot",output.toString());
            result.putString("result",testCapture?"Native button closed panel; background service captured real game tiles; saved capture reopened in native heatmap":"Own application rendered and collector remained accessible");finish(Activity.RESULT_OK,result);
        }catch(Exception e){result.putString("error",e.toString());finish(Activity.RESULT_CANCELED,result);}
    }
}

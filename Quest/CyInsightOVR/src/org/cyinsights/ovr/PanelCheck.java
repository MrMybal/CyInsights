// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.app.*;
import android.content.*;
import android.graphics.Bitmap;
import android.os.*;
import android.view.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.*;
import org.json.*;

/** Development-only check in the normally launched Quest panel, without instrumentation relaunch. */
final class PanelCheck {
    private final MainActivity main;private final Handler ui=new Handler(Looper.getMainLooper());
    PanelCheck(MainActivity main){this.main=main;}
    private void runUi(Runnable action)throws Exception{CountDownLatch done=new CountDownLatch(1);ui.post(()->{try{action.run();}finally{done.countDown();}});if(!done.await(3,TimeUnit.SECONDS))throw new IOException("UI thread unavailable");}
    private void touch(int action,float x,float y)throws Exception{runUi(()->{long t=SystemClock.uptimeMillis();MotionEvent e=MotionEvent.obtain(t,t,action,x,y,0);main.renderViewForTest().dispatchTouchEvent(e);e.recycle();});}
    private void key(int code)throws Exception{runUi(()->main.renderViewForTest().dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN,code)));Thread.sleep(220);}
    private static void check(boolean condition,String why)throws Exception{if(!condition)throw new IOException(why);}
    void start(){new Thread(this::run,"CyOVR-Panel-Check").start();}
    private void run(){JSONObject out=new JSONObject();SharedPreferences prefs=main.getSharedPreferences("MainActivity",0);String original=prefs.getString("display_settings",null);boolean hudStarted=false;
        try{
            long deadline=SystemClock.elapsedRealtime()+18000;JSONObject frame;
            do{frame=main.uiReportForTest();if(frame.optBoolean("activity_active")&&frame.optInt("rendered_frames")>5)break;Thread.sleep(200);}while(SystemClock.elapsedRealtime()<deadline);
            check(frame.optBoolean("activity_active")&&frame.optInt("rendered_frames")>5,"Normally launched panel did not render: "+frame);
            main.pageForTest(2);Thread.sleep(700);key(KeyEvent.KEYCODE_PAGE_UP);key(KeyEvent.KEYCODE_PAGE_UP);key(KeyEvent.KEYCODE_PAGE_UP);
            JSONObject before=main.uiReportForTest();check(before.getDouble("scroll_max")>50,"No real page overflow");int mask=before.getInt("chart_mask");
            float x=(float)before.getDouble("choice_x"),y=(float)before.getDouble("choice_y");
            touch(MotionEvent.ACTION_DOWN,x,y);Thread.sleep(90);touch(MotionEvent.ACTION_MOVE,x,y-110);touch(MotionEvent.ACTION_UP,x,y-110);Thread.sleep(300);
            JSONObject after=main.uiReportForTest();check(after.getDouble("scroll")>50&&after.getInt("chart_mask")==mask,"Drag failed or changed a checkbox: "+after);out.put("drag_scroll_without_click",true);
            key(KeyEvent.KEYCODE_PAGE_UP);check(main.uiReportForTest().getDouble("scroll")<5,"Page-up failed");out.put("key_scroll",true);
            final float wheelX=x,wheelY=y;
            runUi(()->{MotionEvent.PointerProperties p=new MotionEvent.PointerProperties();p.id=0;p.toolType=MotionEvent.TOOL_TYPE_MOUSE;
                MotionEvent.PointerCoords c=new MotionEvent.PointerCoords();c.x=wheelX;c.y=wheelY;c.setAxisValue(MotionEvent.AXIS_VSCROLL,-3);long t=SystemClock.uptimeMillis();
                MotionEvent e=MotionEvent.obtain(t,t,MotionEvent.ACTION_SCROLL,1,new MotionEvent.PointerProperties[]{p},new MotionEvent.PointerCoords[]{c},0,0,1,1,0,0,InputDevice.SOURCE_MOUSE,0);main.renderViewForTest().dispatchGenericMotionEvent(e);e.recycle();});
            Thread.sleep(400);check(main.uiReportForTest().getDouble("scroll")>20,"Scroll-axis input failed");out.put("generic_motion_scroll",true);key(KeyEvent.KEYCODE_PAGE_UP);
            before=main.uiReportForTest();x=(float)before.getDouble("choice_x");y=(float)before.getDouble("choice_y");
            touch(MotionEvent.ACTION_DOWN,x,y);Thread.sleep(120);touch(MotionEvent.ACTION_UP,x,y);Thread.sleep(450);
            after=main.uiReportForTest();check(after.getInt("chart_mask")== (mask^1),"Tap failed: "+after);out.put("rendered_choice_toggled",true);
            JSONObject saved=new JSONObject(prefs.getString("display_settings","{}"));check(saved.getInt("chart_mask")== (mask^1),"Display choice not saved");out.put("choice_saved",true);
            out.put("panel_performance",main.uiReportForTest().optJSONObject("panel_performance"));
            out.put("snapshot_prepare_ms",main.uiReportForTest().optDouble("snapshot_prepare_ms"));
            JSONObject savedCapture=CaptureStore.latest(main);
            if(savedCapture!=null&&"measured".equals(savedCapture.optString("status"))){JSONObject model=main.captureReportForTest();
                check(savedCapture.optString("capture_id").equals(model.optString("capture_id"))&&savedCapture.optInt("tile_count")==model.optInt("tiles"),"Background-prepared capture did not reach the native model");out.put("native_capture_tiles",model.optInt("tiles"));}
            if(main.getIntent().getBooleanExtra("check_hud",false)){
                JSONObject initial=BridgeClient.request(main,new JSONObject().put("operation","snapshot"));check(!initial.optBoolean("meta_overlay_requested"),"Stop the existing managed HUD before this check");
                float[] point=main.hudButtonForTest();touch(MotionEvent.ACTION_DOWN,point[0],point[1]);Thread.sleep(120);touch(MotionEvent.ACTION_UP,point[0],point[1]);
                long until=SystemClock.elapsedRealtime()+15000;while(SystemClock.elapsedRealtime()<until&&!VrHudService.running&&!main.isFinishing())Thread.sleep(100);
                hudStarted=VrHudService.running;check(hudStarted&&!main.isFinishing(),"Starting the HUD closed configuration or failed to start");out.put("configuration_stayed_open",true);
            }
            out.put("result","passed");
        }catch(Exception e){try{out.put("error",e.toString());}catch(JSONException ignored){}}
        finally{
            if(hudStarted)try{BridgeClient.request(main,new JSONObject().put("operation","overlay_meta_off"));runUi(()->main.stopService(new Intent(main,VrHudService.class)));}catch(Exception e){android.util.Log.e("CyInsightOVR","Cannot restore HUD after check",e);}
            SharedPreferences.Editor edit=prefs.edit();if(original==null)edit.remove("display_settings");else edit.putString("display_settings",original);edit.commit();main.reloadDisplaySettingsForTest();
            try{
                Thread.sleep(250);main.pageForTest(2);Thread.sleep(250);
                CountDownLatch done=new CountDownLatch(1);final Bitmap[] image={null};final int[] status={-1};
                runUi(()->{if(!main.renderViewForTest().isShown()){done.countDown();return;}image[0]=Bitmap.createBitmap(main.renderViewForTest().getWidth(),main.renderViewForTest().getHeight(),Bitmap.Config.ARGB_8888);
                    try{PixelCopy.request(main.getWindow(),image[0],code->{status[0]=code;done.countDown();},ui);}catch(IllegalArgumentException invalid){done.countDown();}});
                if(done.await(4,TimeUnit.SECONDS)&&status[0]==PixelCopy.SUCCESS){File file=new File(main.getExternalFilesDir(null),"panel-settings-check.png");try(FileOutputStream f=new FileOutputStream(file)){image[0].compress(Bitmap.CompressFormat.PNG,100,f);}out.put("screenshot",file.toString());}
                out.put("pixel_copy_status",status[0]);
                try(FileOutputStream file=new FileOutputStream(new File(main.getFilesDir(),"panel-input-check.json"))){file.write(out.toString().getBytes(StandardCharsets.UTF_8));}
                android.util.Log.i("CyInsightOVR","Panel input check: "+out);
            }catch(Exception e){android.util.Log.e("CyInsightOVR","Cannot save panel check",e);}
        }
    }
}

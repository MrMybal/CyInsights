// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;

import android.app.*;
import android.content.*;
import android.net.*;
import android.opengl.*;
import android.os.*;
import android.view.*;
import android.widget.EditText;
import org.json.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.*;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public final class MainActivity extends Activity {
    static {System.loadLibrary("cyinsightovr");}
    private static native void nativeInit();
    private static native void nativeResize(int w,int h);
    private static native void nativeDraw();
    private static native void nativeShutdown();
    private static native float[] nativeHudButtonForTest();
    private static native float[] nativeCaptureButtonForTest();
    private static native String nativeCaptureReportForTest();
    static native String nativeOpenXrCapabilities(Context context);
    private static native void nativeTouch(float x,float y,boolean down);
    private static native void nativeHover(float x,float y);
    private static native void nativeWheel(float x,float y,float horizontal,float vertical);
    private static native void nativeScroll(float pixels);
    private static native void nativeSettings(String text);
    private static native void nativeSelectPage(int page);
    private static native String nativeUiReportForTest();
    private static native void nativeSnapshot(String text);
    private static native String nativeCommand();
    private static native void nativeUpdateState(String text);
    private GithubUpdater updater;
    private PanelView view;private PanelInput input;private final Handler ui=new Handler(Looper.getMainLooper());
    private ScheduledFuture<?> pollTask;private int curveHz=10;
    private void schedulePoll(){if(pollTask!=null)pollTask.cancel(false);pollTask=worker.scheduleWithFixedDelay(this::poll,0,1000/curveHz,TimeUnit.MILLISECONDS);}
    private ScheduledExecutorService worker;private volatile boolean active=false;
    private volatile String target="",preparedHeatmapTarget="",lastCapture="",lastActionError="";private String after="0",afterSequence="0",historySession="";private long lastPollNs=0;private boolean highRateCounters=false;private volatile JSONObject capture;
    private JSONObject xrCapabilities;private volatile JSONObject hudCatalog;
    private boolean panelCheckStarted=false;
    private boolean pinRequestPending=false;
    private final Runnable pinRequest=new Runnable(){public void run(){if(pinRequestPending&&active&&!isFinishing()){pinRequestPending=false;QuickAccess.pin(MainActivity.this);}}};
    void reloadDisplaySettingsForTest(){String saved=getPreferences(0).getString("display_settings","{}");view.queueEvent(()->nativeSettings(saved));}
    private void maybeCheckPanel(){
        if(!panelCheckStarted&&getIntent().getBooleanExtra("check_panel",false)&&(getApplicationInfo().flags&android.content.pm.ApplicationInfo.FLAG_DEBUGGABLE)!=0){panelCheckStarted=true;new PanelCheck(this).start();}
    }
    PanelView renderViewForTest(){return view;}
    void pageForTest(int page){view.queueEvent(()->nativeSelectPage(page));}
    private void requestedLaunchActions(){
        if(getIntent().getBooleanExtra("request_view_share",false)){getIntent().removeExtra("request_view_share");startActivity(new Intent(this,ShareViewActivity.class));}
        String selected=getIntent().getStringExtra("target_package");if(selected!=null&&selected.matches("[A-Za-z][A-Za-z0-9_]*(?:\\.[A-Za-z0-9_]+)+")){target=selected;getPreferences(0).edit().putString("target",target).apply();getIntent().removeExtra("target_package");}
        if(getIntent().getBooleanExtra("open_settings",false)){getIntent().removeExtra("open_settings");view.queueEvent(()->nativeSelectPage(2));}
        if(getIntent().getBooleanExtra("open_heatmap_settings",false)){getIntent().removeExtra("open_heatmap_settings");view.queueEvent(()->nativeSelectPage(3));}
        if(getIntent().getBooleanExtra("open_capture",false)){getIntent().removeExtra("open_capture");view.queueEvent(()->nativeSelectPage(1));}
        if(getIntent().getBooleanExtra("pin_configuration",false)){getIntent().removeExtra("pin_configuration");pinRequestPending=true;}
        if(pinRequestPending){ui.removeCallbacks(pinRequest);ui.postDelayed(pinRequest,500);}
    }
    JSONObject uiReportForTest()throws Exception{
        CountDownLatch done=new CountDownLatch(1);final String[] report={null};
        view.queueEvent(()->{report[0]=nativeUiReportForTest();done.countDown();});
        if(!done.await(2,TimeUnit.SECONDS))throw new IOException("Panel GL thread unavailable");return new JSONObject(report[0]).put("last_action_error",lastActionError).put("activity_active",active).put("view",view.diagnostics()).put("panel_performance",view.performance());
    }
    float[] hudButtonForTest()throws Exception{
        CountDownLatch done=new CountDownLatch(1);final float[][] point={null};
        view.queueEvent(()->{point[0]=nativeHudButtonForTest();done.countDown();});
        if(!done.await(2,TimeUnit.SECONDS))throw new IOException("Panel GL thread unavailable");return point[0];
    }
    float[] captureButtonForTest()throws Exception{
        CountDownLatch done=new CountDownLatch(1);final float[][] point={null};
        view.queueEvent(()->{point[0]=nativeCaptureButtonForTest();done.countDown();});
        if(!done.await(2,TimeUnit.SECONDS))throw new IOException("Panel GL thread unavailable");return point[0];
    }
    JSONObject captureReportForTest()throws Exception{
        CountDownLatch done=new CountDownLatch(1);final String[] report={null};
        view.queueEvent(()->{report[0]=nativeCaptureReportForTest();done.countDown();});
        if(!done.await(2,TimeUnit.SECONDS))throw new IOException("Panel GL thread unavailable");return new JSONObject(report[0]);
    }
    private JSONObject request(JSONObject req) throws Exception {
        return BridgeClient.request(this,req);
    }
    private void publish(JSONObject state) {
        JSONObject live=state.optJSONObject("live_tiles");preparedHeatmapTarget=live!=null&&live.optBoolean("prepared")?live.optString("target"):"";
        try{state.put("target_package",target);state.put("android_overlay_attached",OverlayService.windowAttached);state.put("panel_performance",view.performance());if(xrCapabilities!=null)state.put("openxr_capabilities",xrCapabilities);if(hudCatalog!=null)state.put("hud_catalog",hudCatalog);}catch(JSONException ignored){}
        // Called from the worker: JSON parsing and capture preparation stay off GL.
        nativeSnapshot(state.toString());view.requestRender();
    }
    private void poll() {
        if(!active)return;long now=System.nanoTime();if(!highRateCounters&&now-lastPollNs<900000000L)return;lastPollNs=now;
        try {
            JSONObject r=request(new JSONObject().put("operation","snapshot").put("after",after).put("after_sequence",afterSequence).put("history_session",historySession).put("after_capture",lastCapture).put("include_history",true));
            historySession=r.optString("history_session",historySession);JSONObject counterState=r.optJSONObject("draw_counters");highRateCounters=counterState!=null&&counterState.optBoolean("active");
            JSONArray history=r.optJSONArray("history");if(history!=null&&history.length()>0){JSONObject last=history.getJSONObject(history.length()-1);after=last.getString("observed_mono_ns");afterSequence=last.optString("history_sequence",afterSequence);}
            JSONObject c=r.optJSONObject("capture");if(c!=null){capture=c;String id=c.optString("capture_id");if(!id.equals(lastCapture)){CaptureStore.save(this,c);lastCapture=id;}}
            publish(r);
        }catch(Exception e) {
            try{JSONObject offline=new JSONObject().put("bridge",false).put("status","Developer bridge unavailable. Run Start-CyInsightOVR.ps1 once after each headset reboot; then the collector runs on the headset without the PC.").put("metrics",new JSONArray());publish(offline);}catch(JSONException ignored){}
        }
    }
    private void action(String name) {
        android.util.Log.i("CyInsightOVR","UI action: "+name);
        if(name.startsWith("update_")){if(updater!=null)updater.action(name);return;}
        if(name.startsWith("{")){
            try{JSONObject options=new JSONObject(name);
                if("panel_settings".equals(options.optString("operation"))){
                    if(name.length()>4096)throw new IllegalArgumentException("Settings too large");
                    int hz=DrawCounterSample.frequency(options.optInt("curve_hz",10));
                    options.remove("operation");getPreferences(0).edit().putString("display_settings",options.toString()).apply();
                    if(curveHz!=hz){curveHz=hz;schedulePoll();worker.execute(()->{try{JSONObject state=request(new JSONObject().put("operation","snapshot"));JSONObject counters=state.optJSONObject("draw_counters");if(counters!=null&&counters.optBoolean("active"))request(new JSONObject().put("operation","draw_counters_start").put("package",counters.getString("target")).put("hz",hz));}catch(Exception error){lastActionError=error.getMessage();}});}return;
                }
                if(!"overlay_meta_on".equals(options.optString("operation"))&&!"heatmap_start".equals(options.optString("operation"))&&!"heatmap_update".equals(options.optString("operation")))throw new IllegalArgumentException("Invalid HUD action");
                options.put("package",HeatmapTarget.resolve(options.optString("operation"),target,preparedHeatmapTarget));
                send(options);return;
            }catch(Exception e){new AlertDialog.Builder(this).setMessage("Invalid HUD configuration").setPositiveButton("OK",null).show();return;}
        }
        if(name.equals("capture")){
            if(!target.matches("[A-Za-z][A-Za-z0-9_]*(?:\\.[A-Za-z0-9_]+)+")){action("target");return;}
            if(target.equals(getPackageName())){new AlertDialog.Builder(this).setMessage("Select the game package first. This panel closes before capture, so it is not a useful rendering target.").setPositiveButton("Select target",(d,w)->action("target")).setNegativeButton("Cancel",null).show();return;}
            startForegroundService(new Intent(this,CaptureService.class).putExtra("package",target));
            finishAndRemoveTask();return;
        }
        if(name.equals("share_view")){startActivity(new Intent(this,ShareViewActivity.class));return;}
        if(name.equals("stop_share_view")){stopService(new Intent(this,SharedViewService.class));return;}
        if(name.equals("quick_access")){QuickAccess.show(this);return;}
        if(name.equals("inspect_xr")){
            worker.execute(()->{try{
                xrCapabilities=new JSONObject(nativeOpenXrCapabilities(getApplicationContext()));
                xrCapabilities.put("android_overlay_render_permission",checkSelfPermission("com.oculus.permission.ACCESS_OVERLAY_RENDERING")==android.content.pm.PackageManager.PERMISSION_GRANTED?"granted":"denied");
                xrCapabilities.put("vr_overlay_host_permission",checkSelfPermission("com.oculus.vrshell.OVERLAY_HOST")==android.content.pm.PackageManager.PERMISSION_GRANTED?"granted":"denied");
                try(FileOutputStream out=new FileOutputStream(new File(getFilesDir(),"openxr-capabilities.json"))){out.write(xrCapabilities.toString().getBytes(StandardCharsets.UTF_8));}
                poll();
            }catch(Exception e){ui.post(()->{if(!isFinishing())new AlertDialog.Builder(this).setMessage("Runtime inquiry failed: "+e.getMessage()).setPositiveButton("OK",null).show();});}});return;
        }
        if(name.equals("overlay_android")) {
            if(!android.provider.Settings.canDrawOverlays(this))startActivity(new Intent(android.provider.Settings.ACTION_MANAGE_OVERLAY_PERMISSION,android.net.Uri.parse("package:"+getPackageName())));
            else startForegroundService(new Intent(this,OverlayService.class));return;
        }
        if(name.equals("overlay_stop")){stopService(new Intent(this,OverlayService.class));send("overlay_meta_off");return;}
        if(name.equals("target")) {
            EditText field=new EditText(this);field.setSingleLine();field.setText(target);
            new AlertDialog.Builder(this).setTitle("Target Android package").setMessage("Exact package name. Capturing never launches or restarts the target.").setView(field).setNegativeButton("Cancel",null).setPositiveButton("Use target",(d,w)->{
                String value=field.getText().toString().trim();if(value.matches("[A-Za-z][A-Za-z0-9_]*(?:\\.[A-Za-z0-9_]+)+")){target=value;getPreferences(0).edit().putString("target",target).apply();worker.execute(this::poll);}
            }).show();return;
        }
        if(name.equals("export")) {
            if(capture==null){new AlertDialog.Builder(this).setMessage("Capture GPU tiles first.").setPositiveButton("OK",null).show();return;}
            Intent i=new Intent(Intent.ACTION_CREATE_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("application/json").putExtra(Intent.EXTRA_TITLE,"CyInsightOVR-"+lastCapture+".cyovr");startActivityForResult(i,40);return;
        }
        if(name.equals("enable_details")) {
            new AlertDialog.Builder(this).setTitle("Enable detailed GPU profiling?").setMessage("This developer mode adds GPU overhead. It applies to contexts created after activation. Close and reopen the selected target manually. No game will be restarted by CyInsightOVR.").setNegativeButton("Cancel",null).setPositiveButton("Enable for target",(d,w)->send("enable_details")).show();return;
        }
        if(name.equals("heatmap_remove")){new AlertDialog.Builder(this).setTitle("Uninstall heatmap integration?").setMessage("This removes the target setup. Starting again will require preparation and a manual game restart. To hide the HUD and restart it later, use Stop heatmap instead.").setNegativeButton("Cancel",null).setPositiveButton("Uninstall",(d,w)->send("heatmap_remove")).show();return;}
        send(name);
    }
    private void send(String name) {
        try{send(new JSONObject().put("operation",name).put("package",target).put("seconds",.3).put("hz",curveHz));}catch(JSONException ignored){}
    }
    private void send(JSONObject options) {
        worker.execute(()->{
            try {
                JSONObject result=request(options);
                if(result.has("error")){lastActionError=result.optString("error");android.util.Log.w("CyInsightOVR","HUD request "+options.optString("operation")+" for "+options.optString("package")+": "+lastActionError);}else lastActionError="";
                if(result.has("error"))ui.post(()->{if(!isFinishing())new AlertDialog.Builder(this).setMessage(result.optString("error")).setPositiveButton("OK",null).show();});
                else if("hud_catalog".equals(options.optString("operation")))hudCatalog=result;
                else if(("heatmap_start".equals(options.optString("operation"))||"heatmap_update".equals(options.optString("operation")))&&result.optBoolean("active"))ui.post(()->startForegroundService(new Intent(this,LiveHeatmapService.class)));
                else if("heatmap_stop".equals(options.optString("operation"))||"heatmap_remove".equals(options.optString("operation")))ui.post(()->stopService(new Intent(this,LiveHeatmapService.class)));
                else if("overlay_meta_on".equals(options.optString("operation"))&&result.optBoolean("accepted")){
                    ui.post(()->{
                        stopService(new Intent(this,OverlayService.class));
                        startForegroundService(new Intent(this,VrHudService.class));
                        // Starting a HUD does not close the configuration panel.
                    });
                }else if("overlay_meta_off".equals(options.optString("operation")))ui.post(()->stopService(new Intent(this,VrHudService.class)));
                poll();
            }catch(Exception e){ui.post(()->{if(!isFinishing())new AlertDialog.Builder(this).setMessage("Collector request failed: "+e.getMessage()).setPositiveButton("OK",null).show();});poll();}
        });
    }
    private final Runnable commands=new Runnable(){public void run(){if(!active)return;String name=nativeCommand();if(!name.isEmpty())action(name);ui.postDelayed(this,16);}};
    @Override public void onCreate(Bundle b) {
        super.onCreate(b);target=getPreferences(0).getString("target","");getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        view=new PanelView(this,new PanelView.Renderer(){public void create(){nativeInit();}public void resize(int w,int h){nativeResize(w,h);}public void draw(){nativeDraw();}public void destroy(){nativeShutdown();}});
        input=new PanelInput(view,new PanelInput.Sink(){
            public void pointer(float x,float y,boolean down){view.queueEvent(()->nativeTouch(x,y,down));}
            public void hover(float x,float y){view.queueEvent(()->nativeHover(x,y));}
            public void wheel(float x,float y,float horizontal,float vertical){view.queueEvent(()->nativeWheel(x,y,horizontal,vertical));}
            public void scroll(float pixels){view.queueEvent(()->nativeScroll(pixels));}
        });
        view.setFocusableInTouchMode(true);view.setFocusable(true);view.setOnTouchListener(input);view.setOnGenericMotionListener(input);view.setOnKeyListener(input);setContentView(view);view.requestFocus();
        String settings=getPreferences(0).getString("display_settings","{}");view.queueEvent(()->nativeSettings(settings));
        worker=Executors.newSingleThreadScheduledExecutor();
        updater=new GithubUpdater(this,state->{nativeUpdateState(state.toString());view.requestRender();});
        worker.execute(()->{try{capture=CaptureStore.latest(this);if(capture!=null){lastCapture=capture.optString("capture_id");publish(new JSONObject().put("bridge",false).put("status","Opening saved GPU snapshot").put("capture",capture));}}catch(Exception e){android.util.Log.w("CyInsightOVR","Cannot reopen saved snapshot",e);}});
        try{curveHz=DrawCounterSample.frequency(new JSONObject(settings).optInt("curve_hz",10));}catch(Exception ignored){curveHz=10;}schedulePoll();
        worker.execute(()->{try{hudCatalog=request(new JSONObject().put("operation","hud_catalog"));poll();}catch(Exception ignored){}});
    }
    @Override protected void onResume(){super.onResume();lastPollNs=0;active=true;view.onResume();ui.removeCallbacks(commands);ui.post(commands);requestedLaunchActions();if(worker!=null)worker.execute(this::poll);maybeCheckPanel();}
    @Override protected void onNewIntent(Intent intent){super.onNewIntent(intent);setIntent(intent);if(active)requestedLaunchActions();maybeCheckPanel();}
    @Override public void onWindowFocusChanged(boolean focus){super.onWindowFocusChanged(focus);if(focus&&pinRequestPending){ui.removeCallbacks(pinRequest);ui.postDelayed(pinRequest,200);}}
    @Override protected void onPause(){active=false;input.cancel();ui.removeCallbacks(commands);view.onPause();super.onPause();}
    @Override protected void onDestroy(){active=false;if(updater!=null)updater.close();worker.shutdownNow();view.dispose();super.onDestroy();}
    @Override protected void onActivityResult(int requestCode,int resultCode,Intent data){super.onActivityResult(requestCode,resultCode,data);if(requestCode==941&&updater!=null){updater.installationResult(resultCode);return;}if(requestCode==40&&resultCode==RESULT_OK&&data!=null&&capture!=null){try(OutputStream out=getContentResolver().openOutputStream(data.getData())){out.write(capture.toString().getBytes(StandardCharsets.UTF_8));}catch(Exception e){new AlertDialog.Builder(this).setMessage("Export failed: "+e.getMessage()).setPositiveButton("OK",null).show();}}}
}

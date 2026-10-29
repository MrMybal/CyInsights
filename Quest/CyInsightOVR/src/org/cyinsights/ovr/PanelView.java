// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;

import android.content.Context;
import android.graphics.SurfaceTexture;
import android.opengl.*;
import android.os.*;
import android.view.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicBoolean;
import org.json.*;

/** GLES content composed into the host panel's Android window, not a child window. */
final class PanelView extends TextureView implements TextureView.SurfaceTextureListener {
    interface Renderer {void create();void resize(int width,int height);void draw();void destroy();}
    private final HandlerThread thread=new HandlerThread("CyOVR-Panel-GL");
    private final Handler gl;private final Renderer renderer;
    private volatile boolean resumed=false,ready=false;
    private volatile boolean disposed=false;
    private EGLDisplay display=EGL14.EGL_NO_DISPLAY;
    private EGLContext context=EGL14.EGL_NO_CONTEXT;
    private EGLSurface eglSurface=EGL14.EGL_NO_SURFACE;
    private Surface surface;
    private long framePeriodNs=16666667,nextFrameNs=0,lastFrameNs=0,statsStartNs=0,drawTotalNs=0,swapTotalNs=0;
    private int statsFrames=0,intervalCount=0;private final long[] intervals=new long[240];
    private final AtomicBoolean wakeQueued=new AtomicBoolean();private volatile String frameStats="{}";
    private final Runnable frame=new Runnable(){public void run(){
        if(!ready||!resumed)return;
        long start=System.nanoTime();if(statsStartNs==0)statsStartNs=start;
        if(lastFrameNs!=0&&intervalCount<intervals.length)intervals[intervalCount++]=start-lastFrameNs;lastFrameNs=start;
        renderer.draw();long drawn=System.nanoTime();if(!EGL14.eglSwapBuffers(display,eglSurface)){close();return;}long end=System.nanoTime();
        drawTotalNs+=drawn-start;swapTotalNs+=end-drawn;++statsFrames;
        if(end-statsStartNs>=500000000L){
            long[] sorted=java.util.Arrays.copyOf(intervals,intervalCount);java.util.Arrays.sort(sorted);
            try{frameStats=new JSONObject().put("fps",statsFrames*1e9/(end-statsStartNs)).put("target_fps",1e9/framePeriodNs)
                .put("draw_ms",drawTotalNs/1e6/statsFrames).put("swap_wait_ms",swapTotalNs/1e6/statsFrames)
                .put("p95_frame_ms",intervalCount==0?0:sorted[Math.min(intervalCount-1,(int)(intervalCount*.95))]/1e6).toString();}catch(JSONException ignored){}
            statsStartNs=end;drawTotalNs=swapTotalNs=0;statsFrames=intervalCount=0;
        }
        // A deadline includes rendering/presentation time; do not add another full
        // interval after eglSwapBuffers (the previous loop did that).
        if(nextFrameNs==0||end-nextFrameNs>framePeriodNs*2)nextFrameNs=start;
        nextFrameNs+=framePeriodNs;gl.removeCallbacks(this);gl.postDelayed(this,Math.max(1,(nextFrameNs-end+999999)/1000000));
    }};
    PanelView(Context owner,Renderer renderer){
        super(owner);this.renderer=renderer;setOpaque(true);thread.start();gl=new Handler(thread.getLooper());setSurfaceTextureListener(this);
    }
    void queueEvent(Runnable event){if(!disposed){gl.post(event);requestRender();}}
    String diagnostics(){return "ready="+ready+", resumed="+resumed+", texture="+isAvailable()+", shown="+isShown()+", size="+getWidth()+"x"+getHeight();}
    JSONObject performance(){try{return new JSONObject(frameStats);}catch(JSONException ignored){return new JSONObject();}}
    void requestRender(){if(!disposed&&wakeQueued.compareAndSet(false,true))gl.post(()->{wakeQueued.set(false);if(ready&&resumed){gl.removeCallbacks(frame);gl.postDelayed(frame,Math.max(0,(lastFrameNs+framePeriodNs-System.nanoTime()+999999)/1000000));}});}
    void onResume(){resumed=true;gl.post(()->{nextFrameNs=lastFrameNs=statsStartNs=0;drawTotalNs=swapTotalNs=0;statsFrames=intervalCount=0;gl.removeCallbacks(frame);if(ready)gl.post(frame);});}
    void onPause(){resumed=false;gl.removeCallbacks(frame);}
    void dispose(){
        disposed=true;resumed=false;CountDownLatch closed=new CountDownLatch(1);
        gl.post(()->{try{close();}finally{closed.countDown();thread.quitSafely();}});
        // A replacement Activity must not initialize ImGui before this one's
        // GL teardown, otherwise the old renderer could destroy the new context.
        try{if(!closed.await(2,TimeUnit.SECONDS))android.util.Log.e("CyInsightOVR","Panel teardown timed out");}
        catch(InterruptedException e){Thread.currentThread().interrupt();}
    }
    private void open(SurfaceTexture texture,int width,int height){
        close();
        try {
            display=EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY);int[] version=new int[2];
            if(!EGL14.eglInitialize(display,version,0,version,1))throw new IllegalStateException("EGL display initialization failed");
            int[] attrs={EGL14.EGL_RENDERABLE_TYPE,0x40,EGL14.EGL_SURFACE_TYPE,EGL14.EGL_WINDOW_BIT,
                EGL14.EGL_RED_SIZE,8,EGL14.EGL_GREEN_SIZE,8,EGL14.EGL_BLUE_SIZE,8,EGL14.EGL_ALPHA_SIZE,8,EGL14.EGL_NONE};
            EGLConfig[] configs=new EGLConfig[1];int[] count=new int[1];
            if(!EGL14.eglChooseConfig(display,attrs,0,configs,0,1,count,0)||count[0]==0)throw new IllegalStateException("GLES 3 panel configuration unavailable");
            context=EGL14.eglCreateContext(display,configs[0],EGL14.EGL_NO_CONTEXT,new int[]{EGL14.EGL_CONTEXT_CLIENT_VERSION,3,EGL14.EGL_NONE},0);
            surface=new Surface(texture);eglSurface=EGL14.eglCreateWindowSurface(display,configs[0],surface,new int[]{EGL14.EGL_NONE},0);
            if(context==EGL14.EGL_NO_CONTEXT||eglSurface==EGL14.EGL_NO_SURFACE||!EGL14.eglMakeCurrent(display,eglSurface,eglSurface,context))throw new IllegalStateException("Cannot bind panel GL context");
            renderer.create();ready=true;renderer.resize(width,height);if(resumed)gl.post(frame);
        }catch(RuntimeException e){android.util.Log.e("CyInsightOVR","Panel initialization failed",e);close();}
    }
    private void close(){
        gl.removeCallbacks(frame);if(ready)renderer.destroy();ready=false;
        if(display!=EGL14.EGL_NO_DISPLAY){EGL14.eglMakeCurrent(display,EGL14.EGL_NO_SURFACE,EGL14.EGL_NO_SURFACE,EGL14.EGL_NO_CONTEXT);
            if(eglSurface!=EGL14.EGL_NO_SURFACE)EGL14.eglDestroySurface(display,eglSurface);
            if(context!=EGL14.EGL_NO_CONTEXT)EGL14.eglDestroyContext(display,context);EGL14.eglTerminate(display);}
        if(surface!=null)surface.release();surface=null;display=EGL14.EGL_NO_DISPLAY;context=EGL14.EGL_NO_CONTEXT;eglSurface=EGL14.EGL_NO_SURFACE;
    }
    public void onSurfaceTextureAvailable(SurfaceTexture texture,int width,int height){float refresh=getDisplay()==null?60:getDisplay().getRefreshRate();final long period=(long)(1e9/Math.max(30,Math.min(90,refresh)));if(!disposed)gl.post(()->{framePeriodNs=period;if(!disposed)open(texture,width,height);});}
    public void onSurfaceTextureSizeChanged(SurfaceTexture texture,int width,int height){gl.post(()->{if(ready)renderer.resize(width,height);});}
    public boolean onSurfaceTextureDestroyed(SurfaceTexture texture){if(disposed)return true;gl.post(()->{close();texture.release();});return false;}
    public void onSurfaceTextureUpdated(SurfaceTexture texture){}
}

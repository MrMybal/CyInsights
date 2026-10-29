// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.graphics.Bitmap;
import android.util.Base64;
import java.io.*;
import java.nio.*;
import java.util.concurrent.*;
import org.json.*;
/** Explicit optional ADB developer capture. No audio, background stream or gallery files. */
final class QuestViewCapture {
    private static final String TOOL="/system_ext/bin/meta_screencap";
    private static final ScheduledExecutorService TIMER=Executors.newSingleThreadScheduledExecutor(r->{Thread t=new Thread(r,"CyOVR-View-Timeout");t.setDaemon(true);return t;});
    static boolean available(){return new File(TOOL).canExecute();}
    static boolean targetResumed(String dump,String target){for(String line:dump.split("\n")){String value=line.trim();if((value.startsWith("topResumedActivity=")||value.startsWith("mResumedActivity:"))&&value.contains(" "+target+"/"))return true;}return false;}
    static final class Pixels {final int width,height;final int[] argb;Pixels(int w,int h,int[] p){width=w;height=h;argb=p;}}
    static Pixels read(InputStream stream)throws IOException{
        DataInputStream input=new DataInputStream(stream);byte[] header=new byte[16];input.readFully(header);
        ByteBuffer fields=ByteBuffer.wrap(header).order(ByteOrder.LITTLE_ENDIAN);int width=fields.getInt(),height=fields.getInt(),format=fields.getInt();fields.getInt();
        if(width<1||height<1||width>8192||height>8192||(long)width*height>20000000||(format!=1&&format!=2))throw new IOException("Unsupported Meta display capture layout");
        double scale=Math.min(1,Math.min(1280.0/width,720.0/height));int outWidth=Math.max(1,(int)(width*scale)),outHeight=Math.max(1,(int)(height*scale));int[] pixels=new int[outWidth*outHeight];byte[] row=new byte[width*4];int next=0;
        for(int y=0;y<height;++y){input.readFully(row);while(next<outHeight&&(long)next*height/outHeight==y){for(int x=0;x<outWidth;++x){int at=(int)((long)x*width/outWidth)*4;pixels[next*outWidth+x]=0xff000000|((row[at]&255)<<16)|((row[at+1]&255)<<8)|(row[at+2]&255);}++next;}}
        if(input.read()!=-1)throw new IOException("Unexpected bytes after Meta display capture");return new Pixels(outWidth,outHeight,pixels);
    }
    static JSONObject capture(String boot)throws Exception{
        long begin=System.nanoTime();java.lang.Process process=new ProcessBuilder(TOOL).redirectError(ProcessBuilder.Redirect.INHERIT).start();ScheduledFuture<?> timeout=TIMER.schedule(()->process.destroyForcibly(),3,TimeUnit.SECONDS);
        try(InputStream input=process.getInputStream()){
            Pixels pixels=read(input);if(!process.waitFor(500,TimeUnit.MILLISECONDS)||process.exitValue()!=0)throw new IOException("Meta display capture did not complete");long observed=System.nanoTime();
            boolean black=true;for(int pixel:pixels.argb)if((pixel&0xffffff)!=0){black=false;break;}
            Bitmap bitmap=Bitmap.createBitmap(pixels.argb,pixels.width,pixels.height,Bitmap.Config.ARGB_8888);ByteArrayOutputStream jpeg=new ByteArrayOutputStream();try{if(!bitmap.compress(Bitmap.CompressFormat.JPEG,80,jpeg))throw new IOException("Cannot encode Quest image");}finally{bitmap.recycle();}
            if(jpeg.size()>1024*1024)throw new IOException("Quest image exceeds the capture budget");
            return new JSONObject().put("active",true).put("black_frame",black).put("android_boot_id",boot).put("observed_mono_ns",Long.toString(observed)).put("capture_started_mono_ns",Long.toString(begin)).put("capture_finished_mono_ns",Long.toString(observed)).put("source","meta_screencap").put("mapping","composited_view_unregistered").put("width",pixels.width).put("height",pixels.height).put("jpeg_base64",Base64.encodeToString(jpeg.toByteArray(),Base64.NO_WRAP));
        }finally{timeout.cancel(false);if(process.isAlive())process.destroyForcibly();}
    }
}

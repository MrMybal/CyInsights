// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.content.Context;
import android.content.pm.*;
import android.os.ParcelFileDescriptor;
import android.system.*;
import android.os.Process;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;
import org.json.*;

/** Binder grants a private socket pair only to the selected Development target. */
final class AppLayerHost implements AutoCloseable {
    static final int MAGIC=0x43594c31;
    private final Context context;private final DrawCounterMonitor.Sink sink;
    private final Set<ParcelFileDescriptor> sockets=ConcurrentHashMap.newKeySet();
    private final Map<ParcelFileDescriptor,Long> seen=new HashMap<>();private final Semaphore slots=new Semaphore(4);
    private final Map<ParcelFileDescriptor,Integer> protocols=new HashMap<>();
    private volatile boolean closed;private String target="";private int targetUid=-1,counterHz;
    private final ScheduledExecutorService grants=Executors.newSingleThreadScheduledExecutor(r->new Thread(r,"CyOVR-Layer-Grant"));
    private byte[] tiles=new byte[0];private DrawCounterSample parser=new DrawCounterSample();private JSONObject counters;
    private long hudRecorded;private int hudWidth,hudHeight,hudMetrics;
    private byte[] metaPayload=new byte[0];private JSONObject metaReport=new JSONObject();private long metaReportAt;
    final GraphHudController graphs=new GraphHudController();
    AppLayerHost(Context context,DrawCounterMonitor.Sink sink)throws IOException{
        this.context=context.getApplicationContext();this.sink=sink;
        grants.scheduleWithFixedDelay(()->{try{refreshGrant();}catch(Exception error){if(!closed)android.util.Log.w("CyInsightOVR","GPU layer permission refresh failed: "+error.getClass().getSimpleName());}},1,1,TimeUnit.SECONDS);
    }
    private synchronized void refreshGrant(){
        if(closed||targetUid<10000||target.isEmpty())return;
        int modes=android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION|android.content.Intent.FLAG_GRANT_WRITE_URI_PERMISSION;
        // Android revokes temporary URI grants when the target is force-stopped.
        // Restore only our selected target, without shell commands or a PC.
        if(context.checkUriPermission(LayerVisibilityProvider.URI,-1,targetUid,modes)!=PackageManager.PERMISSION_GRANTED)
            context.grantUriPermission(target,LayerVisibilityProvider.URI,modes);
    }
    synchronized void allowTarget(String name)throws Exception{
        if(name.equals(target)){refreshGrant();return;}
        ApplicationInfo info=context.getPackageManager().getApplicationInfo(name,0);
        if((info.flags&ApplicationInfo.FLAG_DEBUGGABLE)==0)throw new IOException("The Vulkan heatmap requires a Development/debuggable target");
        if(info.uid==Process.myUid())throw new IOException("Select the game package rather than CyInsightOVR");
        if(context.checkSelfPermission(DirectMetaHud.PERMISSION)!=PackageManager.PERMISSION_GRANTED)
            throw new IOException("GPU layer configuration needs this app's one-time developer settings authorization");
        int grants=android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION|android.content.Intent.FLAG_GRANT_WRITE_URI_PERMISSION;
        if(!target.isEmpty())context.revokeUriPermission(target,LayerVisibilityProvider.URI,grants);
        context.grantUriPermission(name,LayerVisibilityProvider.URI,grants);
        for(ParcelFileDescriptor socket:sockets)try{socket.close();}catch(IOException ignored){}
        seen.clear();protocols.clear();target=name;targetUid=info.uid;tiles=new byte[0];counterHz=0;counters=null;parser=new DrawCounterSample();hudRecorded=0;metaPayload=new byte[0];metaReport=new JSONObject();metaReportAt=0;graphs.stop();
    }
    synchronized boolean ready(String name){if(!name.equals(target))return false;long now=System.nanoTime();for(long stamp:seen.values())if(now-stamp<3000000000L)return true;return false;}
    synchronized boolean curvesReady(String name){if(!name.equals(target))return false;long now=System.nanoTime();for(Map.Entry<ParcelFileDescriptor,Long> peer:seen.entrySet())if(now-peer.getValue()<3000000000L&&protocols.getOrDefault(peer.getKey(),0)>=4)return true;return false;}
    synchronized void metaPayload(String selected,byte[] bytes)throws IOException{if(!selected.equals(target)||bytes.length>32768)throw new IOException("Invalid target HUD packet");metaPayload=bytes.clone();}
    synchronized JSONObject metaStatus()throws Exception{return new JSONObject(metaReport.toString()).put("layer_connected",curvesReady(target)).put("report_age_ms",metaReportAt==0?JSONObject.NULL:(System.nanoTime()-metaReportAt)/1e6);}
    synchronized JSONObject curveStatus(String name)throws Exception {
        boolean ready=curvesReady(name);long age=hudRecorded==0?Long.MAX_VALUE:Math.max(0,System.nanoTime()-hudRecorded);
        JSONObject status=graphs.status();return status.put("layer_ready",ready).put("commands_recorded",status.optBoolean("active")&&ready&&age<3000000000L).put("recorded_age_ms",hudRecorded==0?JSONObject.NULL:age/1000000.0).put("recorded_metrics",hudMetrics).put("surface_width",hudWidth).put("surface_height",hudHeight);
    }
    synchronized void resetHudReport(){hudRecorded=0;hudMetrics=hudWidth=hudHeight=0;}
    synchronized void publish(String name,byte[] bytes)throws IOException{if(!name.equals(target)||bytes.length>20520)throw new IOException("Invalid local tile publication");tiles=bytes.clone();}
    synchronized void hide(String name){if(name.equals(target))tiles=new byte[0];}
    synchronized void removeTarget(String name)throws Exception{if(!name.equals(target))return;for(ParcelFileDescriptor socket:sockets)try{socket.close();}catch(IOException ignored){}seen.clear();protocols.clear();tiles=new byte[0];counterHz=0;target="";targetUid=-1;graphs.stop();}
    synchronized JSONObject startCounters(String name,int hz)throws Exception{
        DrawCounterSample.frequency(hz);if(!name.equals(target)||targetUid<10000)throw new IOException("Prepare the selected Development Vulkan game first");
        counterHz=hz;counters=null;parser=new DrawCounterSample();return counterStatus();
    }
    synchronized void stopCounters(){counterHz=0;counters=null;parser=new DrawCounterSample();}
    synchronized JSONObject counterStatus()throws Exception{
        JSONObject out=counters==null?new JSONObject().put("status",counterHz==0?"stopped":"waiting").put("reason","Waiting for fresh counters from the target's Vulkan layer"):new JSONObject(counters.toString());
        if(out.has("observed_mono_ns")&&System.nanoTime()-DrawCounterSample.number(out,"observed_mono_ns")>2000000000L)out.put("status","unavailable").put("reason","The target's counter source is stale");
        return out.put("active",counterHz>0).put("target",target).put("requested_hz",counterHz).put("transport","app_local_socket");
    }
    synchronized ParcelFileDescriptor openChannel(int uid,int pid)throws IOException{
        if(closed||uid!=targetUid||uid<10000||pid<=0)throw new SecurityException("Unselected layer caller");
        if(!slots.tryAcquire())throw new IOException("GPU layer connection limit reached");
        ParcelFileDescriptor[] pair;
        try{pair=ParcelFileDescriptor.createSocketPair();}catch(IOException error){slots.release();throw error;}
        ParcelFileDescriptor socket=pair[0];sockets.add(socket);
        Thread peer=new Thread(()->{try{serve(socket,uid,pid);}catch(Exception error){if(!closed)android.util.Log.d("CyInsightOVR","Layer connection ended: "+error.getClass().getSimpleName());}
            finally{sockets.remove(socket);synchronized(this){seen.remove(socket);protocols.remove(socket);}try{socket.close();}catch(IOException ignored){}slots.release();}},"CyOVR-Layer-Peer");
        peer.start();return pair[1];
    }
    private void serve(ParcelFileDescriptor socket,int uid,int pid)throws Exception{
        Os.setsockoptTimeval(socket.getFileDescriptor(),OsConstants.SOL_SOCKET,OsConstants.SO_RCVTIMEO,StructTimeval.fromMillis(5000));
        Os.setsockoptTimeval(socket.getFileDescriptor(),OsConstants.SOL_SOCKET,OsConstants.SO_SNDTIMEO,StructTimeval.fromMillis(5000));
        DataInputStream input=new DataInputStream(new FileInputStream(socket.getFileDescriptor()));DataOutputStream output=new DataOutputStream(new FileOutputStream(socket.getFileDescriptor()));
        if(input.readInt()!=MAGIC)throw new IOException("Unknown layer protocol");int version=input.readInt();if(version<1||version>4)throw new IOException("Unknown layer protocol");int length=input.readInt();if(length<1||length>200)throw new IOException("Invalid layer name");
        byte[] name=new byte[length];input.readFully(name);String process=new String(name,StandardCharsets.UTF_8);String selected;
        synchronized(this){selected=target;if(!process.equals(target)&&!process.startsWith(target+":"))throw new SecurityException("Unselected layer process");protocols.put(socket,version);}
        if(version>=4){byte[] apk=context.getApplicationInfo().sourceDir.getBytes(StandardCharsets.UTF_8);output.writeInt(apk.length);output.write(apk);output.flush();}
        while(!closed){int size=input.readInt();if(size<0||size>16384)throw new IOException("Invalid layer counter size");byte[] data=new byte[size];input.readFully(data);JSONObject measured=null;byte[] payload;int hz;
            synchronized(this){
                if(!target.equals(selected)||uid!=targetUid)throw new SecurityException("Target changed");seen.put(socket,System.nanoTime());
                JSONObject raw=size>0?new JSONObject(new String(data,StandardCharsets.UTF_8)):null;
                if(raw!=null){if(raw.getInt("pid")!=pid)throw new SecurityException("Counter PID does not match its Binder caller");
                    if(version>=3){long stamp=DrawCounterSample.number(raw,"hud_recorded_ns"),now=System.nanoTime();if(stamp>now)throw new IOException("HUD record time is in the future");
                        if(version>=4){JSONObject report=raw.optJSONObject("meta_hud");if(report!=null&&(selected.equals(report.optString("publisher_package"))||!report.has("publisher_package")&&report.has("error"))){metaReport=report;metaReportAt=now;}}
                        int width=raw.optInt("hud_width"),height=raw.optInt("hud_height"),metrics=raw.optInt("hud_metrics");
                        if(stamp>hudRecorded&&now-stamp<3000000000L&&width>=512&&width<=32768&&height>=512&&height<=32768&&metrics>0&&metrics<=GraphHudController.MAX_GRAPHS){hudRecorded=stamp;hudWidth=width;hudHeight=height;hudMetrics=metrics;}
                        raw=raw.optJSONObject("counters");}
                }
                if(raw!=null&&raw.has("version")&&counterHz>0){if(raw.getInt("pid")!=pid)throw new SecurityException("Counter PID does not match its Binder caller");
                    try{JSONObject parsed=parser.accept(raw,System.nanoTime());if(!"unchanged".equals(parsed.optString("status"))){counters=parsed;if("measured".equals(parsed.optString("status")))measured=parsed;}}
                    catch(IllegalArgumentException invalid){parser=new DrawCounterSample();counters=new JSONObject().put("status","unavailable").put("reason",invalid.getMessage());}}
                payload=tiles;hz=counterHz;
            }
            if(measured!=null)sink.sample(measured);
            output.writeInt(MAGIC);output.writeInt(hz);output.writeInt(payload.length);output.write(payload);
            // Statistics render in Meta's existing HUD. The Vulkan layer only
            // collects geometry and draws the separately controlled heatmap.
            if(version>=2){output.writeInt(0);}if(version>=4){byte[] packet;synchronized(this){packet=metaPayload;}output.writeInt(packet.length);output.write(packet);}output.flush();
        }
    }
    @Override public void close(){closed=true;grants.shutdownNow();for(ParcelFileDescriptor socket:sockets)try{socket.close();}catch(IOException ignored){}synchronized(this){seen.clear();tiles=new byte[0];counterHz=0;}}
}

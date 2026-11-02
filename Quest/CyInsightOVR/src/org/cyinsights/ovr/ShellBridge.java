// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;

import android.os.Process;
import java.net.*;
import java.security.*;
import org.json.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;

/** Shared GPU runtime, owned by the APK service; optional legacy shell entry point. */
public final class ShellBridge {
    private android.content.Context application;
    private AppLayerHost layerHost;
    private volatile ServerSocket endpoint;
    private final Set<java.lang.Process> children=ConcurrentHashMap.newKeySet();
    android.os.ParcelFileDescriptor openLayerChannel(int uid,int pid)throws IOException{AppLayerHost host=layerHost;if(host==null)throw new IOException("GPU layer service is starting");return host.openChannel(uid,pid);}
    private final Semaphore exportSlot=new Semaphore(1);private final Set<Socket> exports=ConcurrentHashMap.newKeySet();
    private String authentication;
    private final Object LOCK=new Object();
    private final LinkedHashMap<String,JSONObject> METRICS=new LinkedHashMap<>();
    private final ArrayDeque<JSONObject> HISTORY=new ArrayDeque<>();
    private String historySession=Long.toString(System.nanoTime());
    private long historySequence=0;
    private void appendSample(JSONObject sample)throws Exception{sample.put("history_sequence",Long.toString(++historySequence));METRICS.put(sample.getString("name"),sample);HISTORY.addLast(sample);while(HISTORY.size()>32768)HISTORY.removeFirst();}
    private JSONObject capture,slowSnapshot;
    private long slowSnapshotAt=0;
    private DrawCounterMonitor drawCounters;
    private DrawCounterMonitor createDrawCounters(){DrawCounterMonitor.Runner commands=(args,timeout)->{
        Output value=command(args,timeout);if(!value.complete)throw new IOException("Counter setup command failed");return value.text;
    };return new DrawCounterMonitor(commands,this::drawSample,layerHost);}
    private void drawSample(JSONObject sample)throws Exception{synchronized(LOCK){JSONArray values=sample.getJSONArray("metrics");for(int i=0;i<values.length();++i)appendSample(values.getJSONObject(i));}}
    private void clearDrawHistory(){synchronized(LOCK){METRICS.entrySet().removeIf(e->e.getKey().startsWith("Vulkan "));HISTORY.removeIf(e->e.optString("name").startsWith("Vulkan "));}}
    private volatile boolean capturing=false,running=true;
    private volatile boolean metaOverlayRequested=false;
    private TileRecording recording;
    private final MetaHudController hud=new MetaHudController((args,timeout)->{
        Output output=command(args,timeout);if(!output.complete)throw new IOException("Meta HUD control failed");return output.text;
    },new File("/data/local/tmp/cyinsightovr-meta-hud-original.json"));
    private volatile String selectedMetrics="";
    private volatile java.lang.Process realtime;
    private void terminate(java.lang.Process process){
        if(process==null)return;process.destroy();
        try{if(!process.waitFor(500,TimeUnit.MILLISECONDS)){process.destroyForcibly();process.waitFor(1000,TimeUnit.MILLISECONDS);}}
        catch(InterruptedException e){process.destroyForcibly();Thread.currentThread().interrupt();}
    }
    private volatile String message="Waiting for profiler observations",boot="unknown";
    private final ContinuousTileSource continuousTiles=new ContinuousTileSource();
    private LiveHeatmapController liveTiles;
    private LiveHeatmapController createLiveTiles(){LiveHeatmapController.Runner commands=(args,seconds)->{
        Output data=command(args,seconds);if(!data.complete)throw new IOException("Developer command did not complete");return data.text;
    };LiveHeatmapController.Measure measure=target->{
        for(int i=0;i<30&&realtime!=null;++i)Thread.sleep(50);
        if(realtime!=null)throw new IOException("Counter reader has not released the profiler");
        try{return continuousTiles.next(profiler(),target,boot);}catch(Exception failure){continuousTiles.close();throw failure;}
    };Runnable release=()->{continuousTiles.close();capturing=false;};return application==null?new LiveHeatmapController(commands,measure,release):new LiveHeatmapController(commands,measure,release,application,layerHost);}
    private String profiler() throws FileNotFoundException {
        for(String path:new String[]{"/system_ext/bin/ovrgpuprofiler","/system/bin/ovrgpuprofiler","/vendor/bin/ovrgpuprofiler"})
            if(new File(path).canExecute())return path;
        throw new FileNotFoundException("Meta GPU profiler is not installed on this firmware");
    }
    private String readSmall(String path) {
        try(BufferedReader r=new BufferedReader(new FileReader(path))){return r.readLine().trim();}catch(Exception e){return "unknown";}
    }
    private final class Output {String text;boolean complete;Output(String t,boolean c){text=t;complete=c;}}
    private Output command(String[] args,int seconds) throws Exception {
        java.lang.Process p=new ProcessBuilder(args).redirectErrorStream(true).start();children.add(p);ByteArrayOutputStream output=new ByteArrayOutputStream();
        final boolean[] overflow={false};Thread reader=new Thread(()->{try{byte[] data=new byte[8192];int n;while((n=p.getInputStream().read(data))>=0){if(output.size()+n>6*1024*1024){overflow[0]=true;p.destroyForcibly();break;}output.write(data,0,n);}}catch(IOException ignored){}});
        try{
            reader.start();boolean exited=p.waitFor(seconds,TimeUnit.SECONDS);if(!exited)p.destroyForcibly();reader.join(2000);
            boolean complete=exited&&!reader.isAlive()&&!overflow[0]&&p.exitValue()==0;
            return new Output(new String(output.toByteArray(),StandardCharsets.UTF_8),complete);
        }finally{if(p.isAlive())terminate(p);children.remove(p);}
    }
    private void monitor() {
        while(running) {
            try {
                // Do not run competing real-time and detailed profiler sessions.
                if(capturing){Thread.sleep(100);continue;}
                if(selectedMetrics.isEmpty()){
                    Output catalog=command(new String[]{profiler(),"-m"},5);
                    Set<String> wanted=new HashSet<>(Arrays.asList("GPU Frequency","GPU % Bus Busy","% Time Shading Fragments","% Time Shading Vertices","% Texture Fetch Stall","% Stalled on System Memory"));
                    ArrayList<String> ids=new ArrayList<>();
                    for(String line:catalog.text.split("[\\r\\n]+")){
                        String[] fields=line.trim().split("\\s+",2);
                        if(fields.length==2&&fields[0].matches("[0-9]+")&&wanted.contains(fields[1].trim()))ids.add(fields[0]);
                    }
                    if(!catalog.complete||ids.isEmpty())throw new IOException("Requested GPU metrics are not in the firmware catalog");
                    selectedMetrics=String.join(",",ids);
                }
                if(capturing)continue;
                java.lang.Process p; synchronized(LOCK){if(capturing)continue;p=new ProcessBuilder(profiler(),"-r"+selectedMetrics).redirectErrorStream(true).start();realtime=p;}
                try(BufferedReader r=new BufferedReader(new InputStreamReader(p.getInputStream(),StandardCharsets.UTF_8))) {
                    String line;while(running&&!capturing&&(line=r.readLine())!=null) {
                        JSONObject sample=ProfilerParser.metric(line,System.nanoTime());if(sample==null)continue;
                        synchronized(LOCK){appendSample(sample);}
                        message="Receiving measured system GPU counters";
                    }
                }finally{terminate(p);realtime=null;}
                Thread.sleep(300);
            }catch(Exception e){if(!capturing)message="GPU counters unavailable: "+e.getClass().getSimpleName();try{Thread.sleep(300);}catch(InterruptedException ignored){}}
        }
    }
    private String packageName(JSONObject request) {
        String name=request.optString("package","");if(!name.matches("[A-Za-z][A-Za-z0-9_]*(?:\\.[A-Za-z0-9_]+)+")||name.length()>200)throw new IllegalArgumentException("Select a valid target package first");return name;
    }
    private JSONObject sharedView(boolean pixels){try{String text;if(application==null){Output output=command(new String[]{"/system/bin/run-as","org.cyinsights.ovr","cat","files/shared-view.json"},3);if(!output.complete)return new JSONObject().put("active",false);text=output.text;}else{File file=new File(application.getFilesDir(),"shared-view.json");if(!file.isFile()||file.length()>2*1024*1024)return new JSONObject().put("active",false);text=new String(java.nio.file.Files.readAllBytes(file.toPath()),StandardCharsets.UTF_8);}JSONObject value=new JSONObject(text);
        if(value.optBoolean("active")){
            long age=System.nanoTime()-Long.parseLong(value.optString("observed_mono_ns","0"));
            if(!boot.equals(value.optString("android_boot_id"))||age<0||age>2000000000L){value.put("active",false).put("reason","The shared view is from an old boot or is no longer producing frames; request screen sharing again");value.remove("jpeg_base64");}
        }
        if(!pixels)value.remove("jpeg_base64");return value;}catch(Exception error){return new JSONObject();}}
    private JSONObject recordedView(String target)throws Exception{
        if(application!=null)return sharedView(true).put("target_visibility","not_verified");
        Output before=command(new String[]{"/system/bin/dumpsys","activity","activities"},3);
        if(!before.complete||!QuestViewCapture.targetResumed(before.text,target))return new JSONObject().put("active",false).put("reason","The recorded target is not resumed; another application may be in front");
        JSONObject view=QuestViewCapture.available()?QuestViewCapture.capture(boot):sharedView(true);
        Output after=command(new String[]{"/system/bin/dumpsys","activity","activities"},3);
        if(!after.complete||!QuestViewCapture.targetResumed(after.text,target)){view.put("active",false).put("reason","The target lost focus during image acquisition");view.remove("jpeg_base64");}
        return view;
    }
    private JSONObject dispatch(JSONObject request) throws Exception {
        String op=request.optString("operation","");if(!op.equals("snapshot"))slowSnapshot=null;JSONObject out=new JSONObject();out.put("protocol","cyinsightovr.bridge/1");
        if(op.equals("snapshot")) {
            out.put("status",message);out.put("bridge",true);out.put("service_owner",application==null?"shell":"application");out.put("pc_bootstrap_required",application==null);out.put("capturing",capturing);out.put("android_boot_id",boot);out.put("observed_mono_ns",Long.toString(System.nanoTime()));
            out.put("cross_game_overlay_backend","meta.ovrmetrics.documented_hud");out.put("meta_overlay_requested",metaOverlayRequested);out.put("execution_frame_identity",JSONObject.NULL);
            // Shell commands and target readiness checks remain at 1 Hz even when
            // high-rate draw curves request snapshots twenty times per second.
            if(slowSnapshot==null||System.nanoTime()-slowSnapshotAt>=1000000000L){
                slowSnapshot=new JSONObject().put("vr_hud",application!=null&&DirectMetaHud.available(application)?DirectMetaHud.snapshot(application).optJSONObject("vr_hud"):hud.state()).put("shared_view",sharedView(false)).put("tile_recording",recording.status());
                try{slowSnapshot.put("live_tiles",liveTiles.status());}catch(Exception e){slowSnapshot.put("live_tiles",new JSONObject().put("active",false).put("message",e.getMessage()));}
                slowSnapshotAt=System.nanoTime();
            }
            for(String key:new String[]{"vr_hud","shared_view","tile_recording","live_tiles"})out.put(key,slowSnapshot.opt(key));
            out.put("draw_counters",drawCounters.status());out.put("history_session",historySession);

            out.put("custom_cross_game_hud",new JSONObject().put("status","unavailable")
                .put("reason","No validated public Quest compositor API for a custom HUD over unmodified games. Meta HUD uses the installed Meta renderer."));
            synchronized(LOCK){JSONArray metrics=new JSONArray();for(JSONObject v:METRICS.values())metrics.put(v);out.put("metrics",metrics);
                JSONArray history=new JSONArray();long after=0;try{after=Long.parseLong(request.optString("after","0"));}catch(NumberFormatException ignored){}
                long sequence=request.optLong("after_sequence",-1);if(sequence>=0&&!historySession.equals(request.optString("history_session")))sequence=0;if(request.optBoolean("include_history",false))for(JSONObject v:HISTORY)if(sequence>=0?Long.parseLong(v.getString("history_sequence"))>sequence:Long.parseLong(v.getString("observed_mono_ns"))>after)history.put(v);
                out.put("history",history);out.put("capture",capture==null||capture.optString("capture_id").equals(request.optString("after_capture",""))?JSONObject.NULL:capture);}
        }else if(op.equals("draw_counters_prepare")) {
            drawCounters.close();clearDrawHistory();out=liveTiles.prepare(packageName(request));
        }else if(op.equals("draw_counters_start")) {
            String target=packageName(request);int hz=DrawCounterSample.frequency(request.optInt("hz",10));JSONObject layer=liveTiles.status();
            if(!layer.optBoolean("prepared")||!layer.optBoolean("layer_ready")||!target.equals(layer.optString("target")))throw new IOException("Prepare the selected development Vulkan target, then completely reopen it once before starting draw counters");
            clearDrawHistory();out=drawCounters.start(target,hz);
        }else if(op.equals("draw_counters_stop")) {
            drawCounters.close();clearDrawHistory();out=drawCounters.status();
        }else if(op.equals("capture")) {
            String target=packageName(request);double seconds=request.optDouble("seconds",.3);
            if(!Double.isFinite(seconds)||seconds<.05||seconds>2)throw new IllegalArgumentException("Snapshot duration must be 0.05 to 2 seconds");
            final String job=UUID.randomUUID().toString();
            synchronized(LOCK){if(capturing)throw new IllegalStateException("A GPU snapshot is already running");capturing=true;}
            terminate(realtime);message="Capturing GPU tiles for "+target;
            new Thread(()->{
                try {
                    // Wait for the real-time reader to release the profiler before taking a trace.
                    for(int i=0;i<30&&realtime!=null;++i)Thread.sleep(50);
                    Output data=command(new String[]{profiler(),"-t"+Double.toString(seconds),"-v"},20);
                    JSONObject value=ProfilerParser.capture(data.text,target,boot,Long.toString(System.nanoTime()),data.complete);
                    value.put("capture_id",job);
                    synchronized(LOCK){capture=value;}message=value.getString("reason");
                }catch(Exception e){
                    try{JSONObject failed=ProfilerParser.capture("",target,boot,Long.toString(System.nanoTime()),false);
                        failed.put("capture_id",job);
                        message="GPU snapshot failed: "+e.getClass().getSimpleName();failed.put("reason",message);synchronized(LOCK){capture=failed;}}
                    catch(JSONException ignored){message="GPU snapshot failed";}
                }finally{capturing=false;}
            },"CyOVR-GPU-Snapshot").start();out.put("accepted",true);out.put("capture_id",job);
        }else if(op.equals("companion_capabilities")) {
            boolean nativeImages=application==null&&QuestViewCapture.available();out.put("protocol","cyinsightovr.bridge/2").put("tiles_recording",true).put("image_sharing",true).put("adb_view_capture",nativeImages).put("image_source",nativeImages?"meta_screencap":"mediaprojection").put("storage",application==null?"shell":"app_external").put("service_owner",application==null?"shell":"application").put("shared_view",sharedView(false)).put("tile_recording",recording.status()).put("android_boot_id",boot);
        }else if(op.equals("share_view_request")) {
            if(application!=null)application.startActivity(new android.content.Intent(application,MainActivity.class).addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK).putExtra("request_view_share",true));else command(new String[]{"/system/bin/am","start","-n","org.cyinsights.ovr/.MainActivity","--ez","request_view_share","true"},8);out.put("consent_requested",true);
        }else if(op.equals("tiles_record_start")) {
            String target=packageName(request);JSONObject state=liveTiles.status();
            if(state.optBoolean("active")&&!target.equals(state.optString("target")))throw new IOException("The active HUD belongs to another package");
            if(request.optBoolean("images")&&(application!=null||!QuestViewCapture.available())){JSONObject view=sharedView(false);if(!view.optBoolean("active")||!boot.equals(view.optString("android_boot_id")))throw new IOException("Enable Quest view sharing in CyInsightOVR and accept the system dialog before recording images");}
            boolean acquired=!state.optBoolean("active");
            try{if(acquired){if(!liveTiles.awaitStopped(3000))throw new IOException("The previous GPU capture is stopping");synchronized(LOCK){if(capturing)throw new IOException("Another GPU capture is running");capturing=true;}terminate(realtime);liveTiles.start(new JSONObject().put("package",target).put("record_only",true));}out=recording.start(request);}
            catch(Exception failure){if(acquired){liveTiles.stop();continuousTiles.close();liveTiles.awaitStopped(3000);}throw failure;}
        }else if(op.equals("tiles_record_status")) {out=recording.status();
        }else if(op.equals("tiles_record_stop")) {
            out=recording.stop(request.getString("recording_id"));if(!liveTiles.visible()){liveTiles.stop();continuousTiles.close();liveTiles.awaitStopped(3000);}
        }else if(op.equals("heatmap_prepare")) {
            if(capturing)throw new IllegalStateException("Stop the current capture first");String target=packageName(request);
            Output detail=command(new String[]{profiler(),"-e",target},8);
            if(!detail.complete)throw new IOException("Detailed profiling request failed; the development layer was not prepared");
            out=liveTiles.prepare(target);
            out.put("detailed_request_accepted",detail.complete);
            out.put("message",out.optBoolean("layer_ready")?"The game's GPU layer is connected to CyInsightOVR. Ready for measured tiles and draw counters.":"Layer and detailed profiling requested. Completely close and reopen the target manually once. Target measurements still require verification.");
        }else if(op.equals("heatmap_start")||op.equals("heatmap_update")) {
            String target=packageName(request);JSONObject state=liveTiles.status();
            if(state.optBoolean("active")){
                // A stale UI can send Start twice. Updating the same live target is idempotent.
                if(!target.equals(state.optString("target")))throw new IOException("Stop the current heatmap before selecting another target");
                out=liveTiles.update(request);
            }else{
                if(state.optBoolean("stopping"))continuousTiles.close();
                if(!liveTiles.awaitStopped(3000))throw new IOException("The previous heatmap is still releasing its GPU source");
                synchronized(LOCK){if(capturing)throw new IllegalStateException("A separate GPU snapshot is running; wait for it to finish");capturing=true;}
                terminate(realtime);
                try{out=liveTiles.start(request);}catch(Exception e){capturing=false;throw e;}
            }
        }else if(op.equals("heatmap_stop")||op.equals("heatmap_remove")) {
            if(recording.active()){if(op.equals("heatmap_remove"))throw new IOException("Stop the desktop tile recording before uninstalling its integration");liveTiles.hide();return liveTiles.status();}
            liveTiles.stop();continuousTiles.close();
            if(!liveTiles.awaitStopped(3000))throw new IOException("The GPU source has not stopped yet; target integration was preserved");
            if(op.equals("heatmap_remove")){drawCounters.close();clearDrawHistory();}out=op.equals("heatmap_remove")?liveTiles.remove():liveTiles.status();
        }else if(op.equals("enable_details")) {
            if(capturing)throw new IllegalStateException("Wait for the current snapshot");
            String target=packageName(request);Output r=command(new String[]{profiler(),"-e",target},8);
            message=r.complete?"Detailed profiling requested. Close and reopen the selected target manually before capturing.":"Detailed profiling request failed: "+r.text;
            out.put("status",message);out.put("accepted",r.complete);
        }else if(op.equals("hud_catalog")) {
            out=application==null?hud.catalog():DirectMetaHud.request(application,request);out.put("protocol","cyinsightovr.bridge/1");
        }else if(op.equals("overlay_meta_on")||op.equals("overlay_meta_off")) {
            boolean on=op.equals("overlay_meta_on");
            out=application==null?(on?hud.enable(request):hud.disable()):DirectMetaHud.request(application,request);
            out.put("protocol","cyinsightovr.bridge/1");metaOverlayRequested=on;
            message=on?"Selected HUD settings applied to Meta's renderer. Configuration stays open.":"VR HUD stopped; previous Meta settings restored";
        }else if(op.equals("status")) {
            Output r=command(new String[]{profiler(),"-i"},8);out.put("profiler_status",r.text);out.put("android_boot_id",boot);
        }else throw new IllegalArgumentException("Unknown bridge operation");
        return out;
    }
    public static void main(String[] args) throws Exception {
        if(Process.myUid()!=2000)throw new SecurityException("Start this developer bridge through authorized ADB shell");
        if(args.length!=1)throw new IllegalArgumentException("Expected installed CyInsightOVR application UID");
        int allowed=Integer.parseInt(args[0]);if(allowed<10000)throw new IllegalArgumentException("Invalid app UID");
        ShellBridge runtime=new ShellBridge();Runtime.getRuntime().addShutdownHook(new Thread(runtime::stopLocal,"CyOVR-Stop"));runtime.serve(null,()->{});
    }
    void runLocal(android.content.Context context,Runnable ready)throws Exception{
        if(Process.myUid()!=context.getApplicationInfo().uid)throw new SecurityException("Local GPU service must run as its owning APK");serve(context.getApplicationContext(),ready);
    }
    synchronized void stopLocal(){running=false;for(Socket socket:exports)try{socket.close();}catch(IOException ignored){}ServerSocket server=endpoint;if(server!=null)try{server.close();}catch(IOException ignored){}
        continuousTiles.close();if(liveTiles!=null)try{liveTiles.stop();}catch(Exception ignored){}if(drawCounters!=null)drawCounters.close();terminate(realtime);for(java.lang.Process process:children)terminate(process);if(layerHost!=null)layerHost.close();
        if(recording!=null&&recording.active())try{recording.stop(recording.status().getString("recording_id"));}catch(Exception error){android.util.Log.w("CyInsightOVR","Recording shutdown could not finish",error);}
    }
    private void serve(android.content.Context context,Runnable ready)throws Exception{
        application=context;running=true;capturing=false;selectedMetrics="";historySession=Long.toString(System.nanoTime());synchronized(LOCK){METRICS.clear();HISTORY.clear();historySequence=0;}
        if(context!=null)layerHost=new AppLayerHost(context,this::drawSample);drawCounters=createDrawCounters();liveTiles=createLiveTiles();
        boot=readSmall("/proc/sys/kernel/random/boot_id");
        try{liveTiles.recoverMissingIntegration();}catch(Exception recovery){System.err.println("CyOVR integration recovery: "+recovery);}
        recording=context==null?new TileRecording(this::recordedView,boot):new TileRecording(this::recordedView,boot,new File(context.getExternalFilesDir(null),"Recordings"));liveTiles.observe(value->recording.offer(value));new Thread(this::monitor,"CyOVR-GPU-Counters").start();
        byte[] random=new byte[32];new SecureRandom().nextBytes(random);StringBuilder secret=new StringBuilder();
        for(byte b:random)secret.append(String.format(Locale.ROOT,"%02x",b&255));authentication=secret.toString();
        try(ServerSocket server=new ServerSocket(0,8,InetAddress.getByName("127.0.0.1"))) {
            endpoint=server;
            // The app service publishes its fresh capability only to its own
            // private storage. The optional legacy CLI uses shell-private storage.
            File config=context==null?new File("/data/local/tmp/cyinsightovr-connection.json"):new File(context.getFilesDir(),"developer-bridge.json");
            if(config.exists()&&!config.delete())throw new IOException("Cannot replace bridge connection file");
            if(!config.createNewFile())throw new IOException("Cannot create bridge connection file");
            if(!config.setReadable(false,false)||!config.setWritable(false,false)||!config.setReadable(true,true)||!config.setWritable(true,true))throw new IOException("Cannot protect bridge connection file");
            try(FileOutputStream file=new FileOutputStream(config)){file.write((new JSONObject().put("port",server.getLocalPort()).put("token",authentication).put("android_boot_id",boot).toString()+"\n").getBytes(StandardCharsets.UTF_8));}
            if(context==null)System.out.println("CyInsightOVR developer bridge ready");else ready.run();
            while(running) {
                Socket socket=null;boolean handedOff=false;
                try {
                    socket=server.accept();
                    socket.setSoTimeout(5000);
                    InputStream input=socket.getInputStream();ByteArrayOutputStream line=new ByteArrayOutputStream();int ch;
                    while(line.size()<8192&&(ch=input.read())!=-1&&ch!='\n')line.write(ch);
                    JSONObject result;
                    try{
                        JSONObject request=new JSONObject(new String(line.toByteArray(),StandardCharsets.UTF_8));
                        if(!MessageDigest.isEqual(authentication.getBytes(StandardCharsets.UTF_8),request.optString("authentication","").getBytes(StandardCharsets.UTF_8)))throw new SecurityException("Unauthorized bridge request");
                        if(application!=null&&"recording_export".equals(request.optString("operation"))){
                            final Socket transfer=socket;final File root=new File(application.getExternalFilesDir(null),"Recordings");final JSONObject state=recording.status();final String id=request.getString("recording_id");
                            if(!id.matches("[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}"))throw new IllegalArgumentException("Invalid recording identity");
                            if(!exportSlot.tryAcquire())throw new IOException("A Quest recording export is already running");
                            exports.add(transfer);new Thread(()->{try(Socket peer=transfer){RecordingExport.write(root,state,id,peer.getOutputStream());}
                                catch(Exception error){android.util.Log.w("CyInsightOVR","Recording export failed: "+error.getClass().getSimpleName());}
                                finally{exports.remove(transfer);exportSlot.release();}},"CyOVR-Recording-Export").start();handedOff=true;continue;
                        }
                        result=dispatch(request);
                    }
                    catch(Exception e){result=new JSONObject();result.put("error",e.getMessage()==null?e.getClass().getSimpleName():e.getMessage());}
                    socket.getOutputStream().write((result.toString()+"\n").getBytes(StandardCharsets.UTF_8));socket.getOutputStream().flush();
                }catch(Exception e){System.err.println("CyOVR request: "+e.getClass().getSimpleName());}
                finally{if(socket!=null&&!handedOff)try{socket.close();}catch(IOException ignored){}}
            }
        }finally{stopLocal();endpoint=null;}
    }
}

// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import java.io.*;
import java.nio.*;
import java.nio.charset.StandardCharsets;
import java.security.*;
import java.util.*;
import java.util.concurrent.*;
import org.json.*;
/** Explicit Android development layer and measured, sampled tile feed. Never launches a game. */
final class LiveHeatmapController {
    interface Runner {String run(String[] args,int seconds)throws Exception;}
    interface Measure {JSONObject run(String target)throws Exception;}
    interface Observer {void sample(JSONObject value);}
    private volatile Observer observer;private volatile boolean visible=true;
    void observe(Observer sink){observer=sink;}
    boolean visible(){return visible;}
    synchronized void hide()throws Exception{visible=false;synchronized(publication){if(!target.isEmpty())as(target,"rm","-f",FEED,FEED+".tmp");}}
    private static final String LAYER="VK_LAYER_CYINSIGHTS_heatmap",LIB="libVkLayer_CyInsightOVR.so",FEED="files/cyinsights-live-tiles.bin";
    private static final String[] KEYS={"enable_gpu_debug_layers","gpu_debug_app","gpu_debug_layers","gpu_debug_layer_app"};
    private final Runner runner;private final Measure measure;private final Runnable released;
    private final File stateFile;private final File testLibrary;private final Object publication=new Object();
    private JSONObject prepared;private volatile boolean active=false,stopping=false;
    private volatile String target="",mode="hud",message="Not started",lastId="",surface="";
    private volatile int position=4,tiles=0,width=0,height=0;private volatile double ceiling=500,opacity=.25,hz=0;
    private volatile long lastSample=0;private Thread worker;
    LiveHeatmapController(Runner runner,Measure measure,Runnable released){this(runner,measure,released,new File("/data/local/tmp/cyinsightovr-live-layer-state.json"),null);}
    LiveHeatmapController(Runner runner,Measure measure,Runnable released,File stateFile,File testLibrary){this.runner=runner;this.measure=measure;this.released=released;this.stateFile=stateFile;this.testLibrary=testLibrary;}
    private static void valid(String value){if(value==null||value.length()>200||!value.matches("[A-Za-z][A-Za-z0-9_]*(?:\\.[A-Za-z0-9_]+)+"))throw new IllegalArgumentException("Select a valid Android target package");}
    private String run(String... args)throws Exception{return runner.run(args,5);}
    private String as(String pkg,String... args)throws Exception{String[] call=new String[args.length+2];call[0]="/system/bin/run-as";call[1]=pkg;System.arraycopy(args,0,call,2,args.length);return run(call);}
    private String setting(String key)throws Exception{String value=run("/system/bin/settings","get","global",key).trim();if(!value.matches("[A-Za-z0-9_.:=-]*"))throw new IOException("Unsupported GPU setting format");return value;}
    private void set(String key,String value)throws Exception{if(value.equals("null"))run("/system/bin/settings","delete","global",key);else run("/system/bin/settings","put","global",key,value);}
    private void load()throws Exception{if(prepared!=null||!stateFile.exists())return;if(stateFile.length()>16384)throw new IOException("Invalid saved layer settings");try(BufferedReader in=new BufferedReader(new FileReader(stateFile))){prepared=new JSONObject(in.readLine());valid(prepared.getString("target"));if(prepared.optInt("version")!=1)throw new IOException("Unknown saved layer version");}}
    private boolean installed(String pkg)throws Exception{return as(pkg,"/system/bin/sh","-c","if test -e "+LIB+"; then echo exists; else echo absent; fi").trim().equals("exists");}
    synchronized void recoverMissingIntegration()throws Exception{
        load();if(prepared!=null&&prepared.optBoolean("owns_library")&&!installed(prepared.getString("target"))){
            remove();message="Target integration was missing; previous GPU debug settings restored. Prepare the target again to display a heatmap.";
        }
    }
    private void save()throws Exception{File temp=new File(stateFile+".tmp");if(temp.exists()&&!temp.delete())throw new IOException("Cannot replace layer settings snapshot");if(!temp.createNewFile()||!temp.setReadable(false,false)||!temp.setWritable(false,false)||!temp.setReadable(true,true)||!temp.setWritable(true,true))throw new IOException("Cannot protect layer settings snapshot");try(FileOutputStream out=new FileOutputStream(temp)){out.write(prepared.toString().getBytes(StandardCharsets.UTF_8));}if(!temp.renameTo(stateFile))throw new IOException("Cannot save layer settings snapshot");}
    private File library()throws Exception{if(testLibrary!=null)return testLibrary;String paths=run("/system/bin/pm","path","org.cyinsights.ovr");for(String line:paths.split("[\\r\\n]+"))if(line.startsWith("package:/data/app/")&&line.endsWith("/base.apk")){File apk=new File(line.substring(8));File file=new File(apk.getParentFile(),"lib/arm64/"+LIB);if(file.isFile()&&file.canRead())return file;}throw new IOException("This CyInsightOVR APK does not include the Vulkan development layer");}
    private static String hash(File file)throws Exception{MessageDigest digest=MessageDigest.getInstance("SHA-256");try(InputStream in=new FileInputStream(file)){byte[] data=new byte[8192];int n;while((n=in.read(data))>=0)digest.update(data,0,n);}StringBuilder hex=new StringBuilder();for(byte b:digest.digest())hex.append(String.format(Locale.ROOT,"%02x",b&255));return hex.toString();}
    private String layers()throws Exception{
        // Quest's loader can stop auto-inserting its profiler when explicit debug layers are selected.
        // Keep the installed, enabled system profiler in the explicit chain; never bundle it.
        String socket=run("/system/bin/getprop","debug.vulkan.profiler.namedsocket").trim();
        if(socket.startsWith("/dev/socket/")&&new File("/system_ext/lib64/libVkLayer_ADRENO_qprofiler.so").isFile()){
            // The socket survives -d. Requesting the profiler layer while detailed
            // mode is disabled makes some Quest loaders reject Vulkan entirely.
            String status="";for(String path:new String[]{"/system_ext/bin/ovrgpuprofiler","/system/bin/ovrgpuprofiler","/vendor/bin/ovrgpuprofiler"})if(new File(path).canExecute()){status=run(path,"-i").trim().toLowerCase(Locale.ROOT);break;}
            if(status.contains("profiling is enabled"))return LAYER+":VK_LAYER_ADRENO_qprofiler";
            if(!status.contains("profiling is disabled"))throw new IOException("Cannot confirm the current Meta profiling mode; GPU settings were not changed");
        }
        return LAYER;
    }
    synchronized JSONObject prepare(String pkg)throws Exception{
        valid(pkg);if(active)throw new IOException("Stop the heatmap before preparing another layer");load();recoverMissingIntegration();File source=library();String sha=hash(source),requestedLayers=layers();
        if(prepared!=null){if(pkg.equals(prepared.optString("target"))&&sha.equals(prepared.optString("sha256"))&&requestedLayers.equals(prepared.optString("layers",LAYER)))return status();remove();}
        String root=as(pkg,"pwd").trim();if(!root.endsWith("/"+pkg))throw new IOException("Target must be a debuggable Development APK");
        if(!as(pkg,"/system/bin/sh","-c","if [ -e "+LIB+" ]; then echo exists; else echo absent; fi").trim().equals("absent"))throw new IOException("A target-local Vulkan layer already exists; it will not be replaced");
        JSONObject previous=new JSONObject();for(String key:KEYS)previous.put(key,setting(key));
        prepared=new JSONObject().put("version",1).put("target",pkg).put("sha256",sha).put("layers",requestedLayers).put("previous",previous).put("owns_library",false);save();ArrayList<String> changed=new ArrayList<>();
        try{
            as(pkg,"cp",source.toString(),LIB);prepared.put("owns_library",true);save();as(pkg,"chmod","600",LIB);
            String[] desired={"1",pkg,requestedLayers,"null"};for(int i=0;i<KEYS.length;++i){set(KEYS[i],desired[i]);changed.add(KEYS[i]);}
            for(int i=0;i<KEYS.length;++i)if(!setting(KEYS[i]).equals(desired[i]))throw new IOException("Android did not accept the target layer setting");
            message="Layer prepared. Close and reopen the target manually once; the app never restarts it.";return status();
        }catch(Exception error){boolean restored=true;JSONArray pending=new JSONArray();for(String key:changed)try{set(key,previous.getString(key));}catch(Exception rollback){restored=false;pending.put(key);error.addSuppressed(rollback);}prepared.put("rollback_pending",pending);try{save();}catch(Exception journal){restored=false;error.addSuppressed(journal);}if(prepared.optBoolean("owns_library"))try{as(pkg,"rm","-f",LIB);prepared.put("owns_library",false);save();}catch(Exception rollback){restored=false;error.addSuppressed(rollback);}if(restored){if(!stateFile.delete()&&stateFile.exists())error.addSuppressed(new IOException("Cannot clean layer settings snapshot"));else prepared=null;}else message="Layer preparation failed; original settings snapshot retained for recovery";throw error;}
    }
    private boolean ready(String pkg){try{JSONObject state=new JSONObject(as(pkg,"cat",FEED+".ready"));int pid=state.optInt("pid");if(state.optInt("version")!=1||pid<=0)return false;String pids=run("/system/bin/pidof",pkg).trim();return Arrays.asList(pids.split("\\s+")).contains(Integer.toString(pid));}catch(Exception e){return false;}}
    synchronized JSONObject status()throws Exception{load();JSONObject out=new JSONObject().put("active",active).put("visible",active&&visible).put("stopping",stopping).put("mode",mode).put("position",position).put("opacity",opacity).put("message",message).put("tiles",tiles).put("width",width).put("height",height).put("capture_id",lastId).put("surface_id",surface).put("measured_updates_hz",hz).put("sample_age_ms",lastSample==0?JSONObject.NULL:(System.nanoTime()-lastSample)/1000000.0).put("exact_image_alignment",false).put("per_frame",false);
        out.put("profiler_layer_preserved",prepared!=null&&prepared.optString("layers").contains(":VK_LAYER_ADRENO_qprofiler"));out.put("prepared",prepared!=null).put("target",active||prepared==null?target:prepared.optString("target"));out.put("layer_ready",prepared!=null&&ready(prepared.getString("target")));return out;}
    synchronized JSONObject start(JSONObject options)throws Exception{
        String pkg=options.getString("package");valid(pkg);if(active){if(target.equals(pkg))return update(options);throw new IOException("Stop the heatmap for "+target+" before selecting another target");}if(worker!=null&&worker.isAlive())throw new IOException("The heatmap is still stopping; wait for the collector to release");load();
        if(!options.optBoolean("record_only")&&(prepared==null||!pkg.equals(prepared.optString("target"))||!ready(pkg)))throw new IOException("Prepare the Development Vulkan layer, then reopen the target manually before starting the heatmap");
        String requested=options.optString("mode","hud");if(!requested.equals("hud")&&!requested.equals("fullscreen"))throw new IllegalArgumentException("Unknown heatmap presentation");int pos=options.optInt("position",4);double max=options.optDouble("ceiling_us",500),alpha=options.optDouble("opacity",requested.equals("fullscreen")?.2:.25);
        if(pos<0||pos>8||!Double.isFinite(max)||max<1||max>100000||!Double.isFinite(alpha)||alpha<.05||alpha>1)throw new IllegalArgumentException("Invalid heatmap position, scale or opacity");
        visible=!options.optBoolean("record_only");target=pkg;mode=requested;position=pos;ceiling=max;opacity=alpha;stopping=false;active=true;message="Waiting for fresh measured tiles";tiles=0;lastSample=0;hz=0;
        worker=new Thread(this::stream,"CyOVR-Live-Tiles");worker.start();return status();
    }
    synchronized JSONObject update(JSONObject options)throws Exception{
        if(!active||!target.equals(options.optString("package")))throw new IOException("Start the selected target heatmap before applying settings");
        String requested=options.optString("mode","hud");int pos=options.optInt("position",4);double max=options.optDouble("ceiling_us",500),alpha=options.optDouble("opacity",requested.equals("fullscreen")?.2:.25);
        if((!requested.equals("hud")&&!requested.equals("fullscreen"))||pos<0||pos>8||!Double.isFinite(max)||max<1||max>100000||!Double.isFinite(alpha)||alpha<.05||alpha>1)throw new IllegalArgumentException("Invalid heatmap presentation options");
        if(prepared==null||!target.equals(prepared.optString("target"))||!ready(target))throw new IOException("Prepare and reopen the target before displaying its heatmap");visible=true;mode=requested;position=pos;ceiling=max;opacity=alpha;return status();
    }
    private byte[] encode(JSONObject capture)throws Exception{
        JSONArray surfaces=capture.getJSONArray("surfaces");JSONObject selected=null;long area=0;
        for(int i=0;i<surfaces.length();++i){JSONObject s=surfaces.getJSONObject(i);JSONArray cells=s.getJSONArray("tiles");long size=(long)s.getInt("width")*s.getInt("height");boolean render=false;for(int j=0;j<cells.length();++j)if(cells.getJSONObject(j).getJSONObject("stages_us").has("Render")){render=true;break;}
            if(cells.length()>1&&render&&size>=area){selected=s;area=size;}}
        if(selected==null)throw new IOException("The source did not supply measured Render costs with tile geometry");JSONArray cells=selected.getJSONArray("tiles");if(cells.length()>1024)throw new IOException("Source tile count exceeds this HUD protocol");
        ByteBuffer data=ByteBuffer.allocate(40+20*cells.length()).order(ByteOrder.LITTLE_ENDIAN);long observed=Long.parseLong(capture.getString("observed_mono_ns"));
        data.putInt(0x43595431).putInt(1).putInt(selected.getInt("width")).putInt(selected.getInt("height")).putInt(cells.length()).putInt(position|(mode.equals("fullscreen")?256:0)).putFloat((float)ceiling).putFloat((float)opacity).putLong(observed);
        for(int i=0;i<cells.length();++i){JSONObject t=cells.getJSONObject(i);data.putFloat(t.getInt("x")).putFloat(t.getInt("y")).putFloat(t.getInt("width")).putFloat(t.getInt("height")).putFloat((float)t.getJSONObject("stages_us").optDouble("Render",-1));}
        tiles=cells.length();width=selected.getInt("width");height=selected.getInt("height");surface=selected.getString("id");lastId=capture.optString("capture_id");lastSample=observed;return data.array();
    }
    private void publish(byte[] bytes)throws Exception{synchronized(publication){if(!active)return;java.lang.Process process=new ProcessBuilder("/system/bin/run-as",target,"/system/bin/sh","-c","umask 077; cat > "+FEED+".tmp && mv "+FEED+".tmp "+FEED).redirectErrorStream(true).start();
        try{try(OutputStream out=process.getOutputStream()){out.write(bytes);}if(!process.waitFor(3,TimeUnit.SECONDS)||process.exitValue()!=0)throw new IOException("Cannot publish target-local heatmap input");}finally{if(process.isAlive())process.destroyForcibly();}}}
    private void stream(){long previous=0;
        try{while(active){try{JSONObject sample=measure.run(target);if(!active)break;if(!"measured".equals(sample.optString("status"))){message=sample.optString("reason","Fresh tile data unavailable");Thread.sleep(300);continue;}Observer sink=observer;if(sink!=null)sink.sample(sample);byte[] data=encode(sample);if(!active)break;if(visible)publish(data);long now=System.nanoTime();if(previous!=0)hz=1e9/(now-previous);previous=now;message="Measured Render costs per tile. Sampled updates; image/frame association is approximate.";Thread.sleep(150);}catch(Exception error){if(!active)break;message="Tile feed unavailable: "+(error.getMessage()==null?error.getClass().getSimpleName():error.getMessage());Thread.sleep(500);}}}
        catch(InterruptedException interrupted){Thread.currentThread().interrupt();}
        finally{active=false;stopping=true;try{as(target,"rm","-f",FEED,FEED+".tmp");}catch(Exception ignored){}try{released.run();}finally{stopping=false;}}
    }
    synchronized JSONObject stop()throws Exception{active=false;stopping=worker!=null&&worker.isAlive();synchronized(publication){if(!target.isEmpty())as(target,"rm","-f",FEED,FEED+".tmp");}message=stopping?"Stopping capture; new render recordings no longer receive tile data":"Heatmap stopped";return status();}
    boolean awaitStopped(long milliseconds)throws InterruptedException{Thread previous;synchronized(this){previous=worker;}if(previous!=null&&previous!=Thread.currentThread())previous.join(milliseconds);boolean done=previous==null||!previous.isAlive();if(done&&!active&&message.startsWith("Stopping"))message="Heatmap stopped. The target remains prepared for Start heatmap.";return done;}
    synchronized JSONObject remove()throws Exception{stop();load();if(prepared==null)return status();if(worker!=null&&worker.isAlive())throw new IOException("Wait for the active capture to stop before removing the layer");String pkg=prepared.getString("target");JSONObject previous=prepared.getJSONObject("previous");String[] desired={"1",pkg,prepared.optString("layers",LAYER),"null"};boolean ours=true;for(int i=0;i<KEYS.length;++i)if(!setting(KEYS[i]).equals(desired[i]))ours=false;
        JSONArray pending=prepared.optJSONArray("rollback_pending");
        if(pending!=null){for(int j=0;j<pending.length();++j){String key=pending.getString(j);int index=Arrays.asList(KEYS).indexOf(key);if(index<0)throw new IOException("Invalid recovery setting");if(setting(key).equals(desired[index]))set(key,previous.getString(key));}}
        else if(ours)for(String key:KEYS)set(key,previous.getString(key));
        if(prepared.optBoolean("owns_library")){if(installed(pkg)){String actual=as(pkg,"sha256sum",LIB).trim().split("\\s+")[0];if(!actual.equals(prepared.getString("sha256")))throw new IOException("Target layer changed; refusing to remove an unrecognized file");}as(pkg,"rm","-f",LIB,FEED,FEED+".tmp",FEED+".ready","files/cyinsights-draw-counters.json","files/cyinsights-draw-counters.json.tmp","files/cyinsights-draw-counters.json.enabled","files/cyinsights-draw-counters.json.enabled.tmp");}
        if(!stateFile.delete()&&stateFile.exists())throw new IOException("Cannot remove layer settings snapshot");prepared=null;message=ours?"Development layer removed; previous GPU settings restored":"Layer removed. Another tool changed GPU settings; those settings were preserved.";return status();
    }
}

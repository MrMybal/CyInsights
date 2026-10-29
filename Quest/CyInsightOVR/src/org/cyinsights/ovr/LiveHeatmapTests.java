// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import java.io.*;import java.util.*;import java.security.*;import java.util.concurrent.atomic.*;import org.json.*;
/** Explicit simulated lifecycle checks. No real target, GPU setting or measured data is changed. */
public final class LiveHeatmapTests {
    static void check(boolean yes,String message){if(!yes)throw new AssertionError(message);}
    static final class Fake implements LiveHeatmapController.Runner {
        final Map<String,String> settings=new HashMap<>();boolean debuggable=true,library=false,ready=false,fail=false,rollbackFail=false,tampered=false,profile=false,detailed=true;String sha;
        Fake(String sha){this.sha=sha;for(String key:new String[]{"enable_gpu_debug_layers","gpu_debug_app","gpu_debug_layers","gpu_debug_layer_app"})settings.put(key,"null");}
        public String run(String[] a,int timeout)throws Exception{
            if(a[0].endsWith("settings")){String op=a[1],key=a[3];if(op.equals("get"))return settings.get(key);if(rollbackFail&&key.equals("enable_gpu_debug_layers")&&op.equals("delete"))throw new IOException("Simulated rollback failure");if(fail&&key.equals("gpu_debug_layers")){fail=false;throw new IOException("Simulated settings failure");}settings.put(key,op.equals("delete")?"null":a[4]);return "";}
            if(a[0].endsWith("getprop"))return profile?"/dev/socket/explicit-test-profiler":"";
            if(a[0].endsWith("ovrgpuprofiler"))return detailed?"Detailed GPU profiling is enabled":"Detailed GPU profiling is disabled";
            if(a[0].endsWith("pidof"))return Integer.toString(android.os.Process.myPid());
            if(!a[0].endsWith("run-as"))throw new AssertionError("Unexpected command in simulated test");
            if(!debuggable)throw new IOException("Not debuggable");String command=a[2];
            if(command.equals("pwd"))return "/data/user/0/org.cyinsights.fixture";
            if(command.endsWith("sh"))return library?"exists":"absent";
            if(command.equals("cp")){library=true;return "";}
            if(command.equals("chmod"))return "";
            if(command.equals("cat")){if(!ready)throw new IOException("Not ready");return new JSONObject().put("version",1).put("pid",android.os.Process.myPid()).toString();}
            if(command.equals("sha256sum"))return (tampered?"modified":sha)+"  layer.so";
            if(command.equals("rm")){if(Arrays.asList(a).contains("libVkLayer_CyInsightOVR.so"))library=false;return "";}
            throw new AssertionError("Unexpected simulated target command");
        }
    }
    public static void main(String[] args)throws Exception{
        File state=new File("/data/local/tmp/cyovr-live-state-test-"+android.os.Process.myPid());File fixture=new File(state+".fixture");if(state.exists()||fixture.exists())throw new IOException("Test files already exist");
        try{byte[] bytes="Explicit test library fixture, never loaded".getBytes("UTF-8");try(FileOutputStream out=new FileOutputStream(fixture)){out.write(bytes);}StringBuilder sha=new StringBuilder();for(byte b:MessageDigest.getInstance("SHA-256").digest(bytes))sha.append(String.format(Locale.ROOT,"%02x",b&255));Fake fake=new Fake(sha.toString());Map<String,String> original=new HashMap<>(fake.settings);AtomicBoolean released=new AtomicBoolean();AtomicInteger observed=new AtomicInteger();
            LiveHeatmapController control=new LiveHeatmapController(fake,pkg->{observed.incrementAndGet();return new JSONObject().put("status","unavailable").put("reason","Explicit simulated source");},()->released.set(true),state,fixture);
            try{control.prepare("invalid;target");throw new AssertionError("Invalid package accepted");}catch(IllegalArgumentException expected){}check(!state.exists()&&fake.settings.equals(original),"Invalid package mutated state");
            fake.debuggable=false;try{control.prepare("org.cyinsights.fixture");throw new AssertionError("Non-debuggable target accepted");}catch(IOException expected){}fake.debuggable=true;check(!state.exists(),"Non-debuggable target created state");
            fake.library=true;try{control.prepare("org.cyinsights.fixture");throw new AssertionError("Existing layer overwritten");}catch(IOException expected){}fake.library=false;check(fake.settings.equals(original),"Existing-layer rejection changed GPU settings");
            JSONObject prepared=control.prepare("org.cyinsights.fixture");check(prepared.getBoolean("prepared")&&!prepared.getBoolean("layer_ready")&&fake.library&&state.isFile(),"Preparation did not retain owned settings");
            control.remove();check(fake.settings.equals(original)&&!fake.library&&!state.exists(),"Original settings not restored");
            fake.profile=true;JSONObject chain=control.prepare("org.cyinsights.fixture");boolean installed=new File("/system_ext/lib64/libVkLayer_ADRENO_qprofiler.so").isFile();check(chain.getBoolean("profiler_layer_preserved")==installed,"Enabled installed profiler was not preserved");check(fake.settings.get("gpu_debug_layers").contains(":VK_LAYER_ADRENO_qprofiler")==installed,"Explicit chain omitted the system profiler");control.remove();check(fake.settings.equals(original),"Combined layer removal lost original settings");fake.detailed=false;
            JSONObject light=control.prepare("org.cyinsights.fixture");check(!light.getBoolean("profiler_layer_preserved"),"A surviving socket must not enable a disabled profiler layer");check(!fake.settings.get("gpu_debug_layers").contains(":VK_LAYER_ADRENO_qprofiler"),"Disabled detailed profiler broke the lightweight chain");control.remove();check(fake.settings.equals(original),"Lightweight chain did not restore original settings");fake.profile=false;fake.detailed=true;
            fake.fail=true;try{control.prepare("org.cyinsights.fixture");throw new AssertionError("Injected preparation failure hidden");}catch(IOException expected){}check(fake.settings.equals(original)&&!fake.library&&!state.exists(),"Partial setup rollback failed");
            fake.fail=true;fake.rollbackFail=true;try{control.prepare("org.cyinsights.fixture");throw new AssertionError("Preparation failure hidden");}catch(IOException expected){}check(state.exists()&&!fake.library,"Failed rollback discarded recovery journal");fake.rollbackFail=false;control.remove();check(!state.exists()&&fake.settings.equals(original),"Retained journal did not restore the pending setting");
            control.prepare("org.cyinsights.fixture");fake.settings.put("gpu_debug_app","another.tool");Map<String,String> changed=new HashMap<>(fake.settings);control.remove();check(fake.settings.equals(changed)&&!fake.library,"Another tool's settings were overwritten");fake.settings.clear();fake.settings.putAll(original);
            control.prepare("org.cyinsights.fixture");fake.tampered=true;try{control.remove();throw new AssertionError("Changed library removed");}catch(IOException expected){}check(fake.library&&state.exists(),"Changed library was not preserved");fake.tampered=false;control.remove();
            control.prepare("org.cyinsights.fixture");JSONObject options=new JSONObject().put("package","org.cyinsights.fixture").put("mode","hud").put("position",8).put("opacity",.8);
            try{control.start(options);throw new AssertionError("Missing running layer accepted");}catch(IOException expected){}fake.ready=true;
            try{control.start(new JSONObject(options.toString()).put("mode","bad"));throw new AssertionError("Unknown mode accepted");}catch(IllegalArgumentException expected){}check(!control.status().getBoolean("active"),"Rejected mode activated feed");
            check(control.start(options).getBoolean("active"),"Valid live request not started");check(control.start(options).getBoolean("active"),"Repeated start of the same target was not idempotent");long until=System.currentTimeMillis()+2000;while(observed.get()==0&&System.currentTimeMillis()<until)Thread.sleep(20);check(observed.get()>0,"Worker did not request its source");
            JSONObject full=control.update(new JSONObject(options.toString()).put("mode","fullscreen").put("position",6).put("opacity",.3));check(full.getString("mode").equals("fullscreen")&&full.getInt("position")==6,"Hot mode/placement update failed");
            try{control.update(new JSONObject(options.toString()).put("opacity",2));throw new AssertionError("Invalid opacity accepted");}catch(IllegalArgumentException expected){}check(control.status().getString("mode").equals("fullscreen"),"Rejected update mutated mode");
            control.stop();until=System.currentTimeMillis()+2000;while(!released.get()&&System.currentTimeMillis()<until)Thread.sleep(20);check(released.get()&&!control.status().getBoolean("active"),"Stop did not release collector");released.set(false);check(control.start(options).getBoolean("active"),"Stopped prepared target could not restart");control.stop();check(control.awaitStopped(2000)&&released.get(),"Restarted worker was not fully released");control.remove();check(!state.exists()&&!fake.library&&fake.settings.equals(original),"Final cleanup failed");
            control.prepare("org.cyinsights.fixture");fake.library=false;control.recoverMissingIntegration();check(!state.exists()&&fake.settings.equals(original),"Missing library left a forced unavailable Vulkan layer");
            control.prepare("org.cyinsights.fixture");fake.library=false;fake.settings.put("gpu_debug_app","another.tool");changed=new HashMap<>(fake.settings);control.remove();check(!state.exists()&&fake.settings.equals(changed),"Missing-layer cleanup changed another tool's settings");
            System.out.println("26 live heatmap lifecycle checks passed (explicit simulated service; no rendering or hardware claim)");
        }finally{state.delete();new File(state+".tmp").delete();fixture.delete();}
    }
}

// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.app.*;
import android.os.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.*;
import org.json.*;

/** Read-only profiler capability inquiry under the real APK UID. */
public final class LocalProfilerProbe extends Instrumentation {
    @Override public void onCreate(Bundle args){super.onCreate(args);start();}
    private JSONObject command(String executable,String argument)throws Exception{
        java.lang.Process process=new ProcessBuilder(executable,argument).redirectErrorStream(true).start();
        ByteArrayOutputStream bytes=new ByteArrayOutputStream();Thread reader=new Thread(()->{try{byte[] b=new byte[4096];int n;while((n=process.getInputStream().read(b))!=-1){if(bytes.size()+n>256*1024){process.destroy();break;}bytes.write(b,0,n);}}catch(IOException ignored){}},"CyOVR-Profiler-Probe");reader.start();
        boolean exited=process.waitFor(5,TimeUnit.SECONDS);if(!exited)process.destroyForcibly();reader.join(1500);
        String text=new String(bytes.toByteArray(),StandardCharsets.UTF_8);
        return new JSONObject().put("completed",exited&&!reader.isAlive()).put("exit",exited?process.exitValue():-1).put("output",text);
    }
    @Override public void onStart(){Bundle result=new Bundle();try{
        if(android.os.Process.myUid()==2000)throw new SecurityException("Probe must run as the application");
        JSONObject report=new JSONObject().put("runs_as_application",true);String executable="";
        for(String path:new String[]{"/system_ext/bin/ovrgpuprofiler","/system/bin/ovrgpuprofiler","/vendor/bin/ovrgpuprofiler"})if(new File(path).canExecute()){executable=path;break;}
        if(executable.isEmpty())throw new FileNotFoundException("Installed profiler is not executable by this APK");
        report.put("status",command(executable,"-i")).put("catalog",command(executable,"-m"));
        try(OutputStream file=new FileOutputStream(new File(getTargetContext().getFilesDir(),"local-profiler-probe.json"))){file.write(report.toString().getBytes(StandardCharsets.UTF_8));}
        result.putBoolean("runs_as_application",true);for(String key:new String[]{"status","catalog"}){JSONObject value=report.getJSONObject(key);result.putBoolean(key+"_completed",value.getBoolean("completed"));result.putInt(key+"_exit",value.getInt("exit"));}
        result.putString("result","Read-only inquiry completed");finish(Activity.RESULT_OK,result);
    }catch(Exception error){result.putString("error",error.toString());finish(Activity.RESULT_CANCELED,result);}}
}

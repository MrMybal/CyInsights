// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.TimeUnit;
import org.json.*;

final class DrawCounterMonitor implements AutoCloseable {
    interface Runner {String run(String[] args,int seconds)throws Exception;}
    interface Sink {void sample(JSONObject sample)throws Exception;}
    private final Runner runner;private final Sink sink;
    private static final class Session {volatile boolean closed=false;volatile Process process;Thread worker;String target;int hz;volatile JSONObject latest;}
    private volatile Session current;
    DrawCounterMonitor(Runner runner,Sink sink){this.runner=runner;this.sink=sink;}
    synchronized JSONObject start(String target,int hz)throws Exception{
        DrawCounterSample.frequency(hz);if(!target.matches("[A-Za-z][A-Za-z0-9_]*(?:\\.[A-Za-z0-9_]+)+"))throw new IllegalArgumentException("Invalid counter target");
        if(current!=null&&current.target.equals(target)&&current.hz==hz&&!current.closed)return status();
        close();String apk="";for(String line:runner.run(new String[]{"/system/bin/pm","path","org.cyinsights.ovr"},5).split("[\\r\\n]+"))
            if(line.startsWith("package:/data/app/")&&line.endsWith("/base.apk"))apk=line.substring(8);
        if(apk.isEmpty())throw new IOException("Cannot locate the installed companion reader");
        Session session=new Session();session.target=target;session.hz=hz;
        session.process=new ProcessBuilder("/system/bin/run-as",target,"/system/bin/env","CLASSPATH="+apk,"app_process","/system/bin","--nice-name=cyinsights-draw-reader","org.cyinsights.ovr.DrawCounterStream",target,Integer.toString(hz)).redirectErrorStream(true).start();
        current=session;session.worker=new Thread(()->read(session),"CyOVR-Draw-Counters");session.worker.start();return status();
    }
    private void read(Session session){DrawCounterSample parser=new DrawCounterSample();
        try(BufferedReader input=new BufferedReader(new InputStreamReader(session.process.getInputStream(),StandardCharsets.UTF_8))){String line;
            while(!session.closed&&(line=input.readLine())!=null){if(line.length()>16384)throw new IOException("Counter stream response exceeds limit");
                if(!line.startsWith("{"))continue;JSONObject raw=new JSONObject(line),value;
                if(raw.has("source_error"))value=new JSONObject().put("status","unavailable").put("reason",raw.getString("source_error"));
                else try{value=parser.accept(raw,System.nanoTime());}
                catch(Exception temporarilyInvalid){parser=new DrawCounterSample();value=new JSONObject().put("status","unavailable").put("reason","Waiting for fresh target counters: "+temporarilyInvalid.getMessage());}
                if("unchanged".equals(value.optString("status")))continue;
                session.latest=value;if(!session.closed&&current==session&&"measured".equals(value.optString("status")))sink.sample(value);
            }
            if(!session.closed)session.latest=new JSONObject().put("status","unavailable").put("reason","Target-local counter reader stopped. Check the target and installed development layer.");
        }catch(Exception error){try{session.latest=new JSONObject().put("status","unavailable").put("reason","Draw counter stream failed: "+error.getMessage());}catch(Exception ignored){}}
        finally{try{session.process.getOutputStream().close();}catch(IOException ignored){}}
    }
    JSONObject status()throws Exception{
        Session s=current;if(s==null)return new JSONObject().put("active",false).put("status","stopped");
        JSONObject value=s.latest==null?new JSONObject().put("status","waiting").put("reason","Waiting for the development Vulkan layer"):new JSONObject(s.latest.toString());
        if(value.has("observed_mono_ns")&&System.nanoTime()-DrawCounterSample.number(value,"observed_mono_ns")>2000000000L)value.put("status","unavailable").put("reason","Draw counter stream is stale");
        return value.put("active",!s.closed&&s.process.isAlive()).put("target",s.target).put("requested_hz",s.hz);
    }
    @Override public synchronized void close(){Session s=current;current=null;if(s==null)return;s.closed=true;
        try{s.process.getOutputStream().close();}catch(IOException ignored){}
        try{if(!s.process.waitFor(1500,TimeUnit.MILLISECONDS)){s.process.destroyForcibly();s.process.waitFor(500,TimeUnit.MILLISECONDS);}if(s.worker!=null)s.worker.join(1000);}catch(InterruptedException error){Thread.currentThread().interrupt();}
    }
}

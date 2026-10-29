// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.TimeUnit;
import org.json.*;

/** Real target-UID lifecycle checks. No GPU data is fabricated or written. */
public final class DrawCounterReaderTests {
    private static String run(String[] args,int timeout)throws Exception{
        Process process=new ProcessBuilder(args).redirectErrorStream(true).start();
        if(!process.waitFor(timeout,TimeUnit.SECONDS)){process.destroyForcibly();throw new IOException("Test command timed out");}
        ByteArrayOutputStream text=new ByteArrayOutputStream();byte[] buffer=new byte[4096];int n;while((n=process.getInputStream().read(buffer))!=-1)text.write(buffer,0,n);
        return new String(text.toByteArray(),StandardCharsets.UTF_8);
    }
    private static String readers()throws Exception{return run(new String[]{"/system/bin/pidof","cyinsights-draw-reader"},3).trim();}
    public static void main(String[] args)throws Exception{
        if(args.length!=1||!"org.cyinsights.gputest".equals(args[0]))throw new IllegalArgumentException("Use only the dedicated GPU test project");
        String baseline=readers();int cycles=0;
        try(DrawCounterMonitor monitor=new DrawCounterMonitor(DrawCounterReaderTests::run,value->{})){
            for(int hz:new int[]{5,20,5}){
                monitor.start(args[0],hz);Thread.sleep(600);JSONObject state=monitor.status();
                if(!state.optBoolean("active")||(!"measured".equals(state.optString("status"))&&!state.optString("reason").startsWith("Draw counter source unavailable")&&!state.optString("reason").startsWith("Waiting for fresh target counters:")))throw new AssertionError("Target-UID reader did not run: "+state);
                monitor.close();Thread.sleep(100);
                if(!baseline.equals(readers()))throw new AssertionError("Stopping left a target-UID reader process behind");
                ++cycles;
            }
        }
        System.out.println(cycles+" real target-UID reader start/stop cycles passed; no GPU output claimed");
    }
}

// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import java.io.*;import java.nio.charset.StandardCharsets;import java.util.concurrent.*;import java.util.regex.*;import org.json.*;
/** One owned profiler process, complete target sections, bounded latest-only handoff. */
final class ContinuousTileSource implements AutoCloseable {
    interface Sink {void accept(String text)throws Exception;}
    static final class Sections {
        private static final Pattern PROCESS=Pattern.compile("^Process\\s+(\\S+)\\s+has\\s+.*");
        private final String target;private final Sink sink;private StringBuilder current;private boolean overflow;
        Sections(String target,Sink sink){this.target=target;this.sink=sink;}
        void line(String line)throws Exception{
            java.util.regex.Matcher header=PROCESS.matcher(line.trim());
            if(header.matches()){
                if(current!=null&&!overflow)sink.accept(current.toString());
                current=header.group(1).equals(target)?new StringBuilder():null;overflow=false;
            }
            if(current!=null&&!overflow){if(current.length()+line.length()>2*1024*1024){overflow=true;current=null;}else current.append(line).append('\n');}
        }
        // No EOF flush: the last section may have been interrupted or truncated.
    }
    private java.lang.Process process;private Thread reader;private String target="",error="";
    private JSONObject latest;private long sequence,delivered;private boolean closed;
    synchronized JSONObject next(String executable,String requested,String boot)throws Exception{
        if(process==null){closed=false;target=requested;error="";sequence=delivered=0;latest=null;
            process=new ProcessBuilder(executable,"-t0.1","-v","-c").redirectErrorStream(true).start();final java.lang.Process owned=process;
            reader=new Thread(()->{
                try(BufferedReader input=new BufferedReader(new InputStreamReader(owned.getInputStream(),StandardCharsets.UTF_8))){
                    Sections sections=new Sections(requested,text->{JSONObject sample=ProfilerParser.capture(text,requested,boot,Long.toString(System.nanoTime()),true);
                        synchronized(ContinuousTileSource.this){if(!closed&&process==owned){latest=sample;++sequence;ContinuousTileSource.this.notifyAll();}}});
                    StringBuilder diagnostic=new StringBuilder();String line;while((line=input.readLine())!=null){if(diagnostic.length()<512)diagnostic.append(line,0,Math.min(line.length(),512-diagnostic.length())).append(" | ");if(line.length()>65536)throw new IOException("Profiler line exceeded the response bound");sections.line(line);}
                    synchronized(ContinuousTileSource.this){if(!closed&&process==owned)error="Continuous GPU profiler exited ("+owned.waitFor()+"): "+diagnostic;}
                }catch(Exception failure){synchronized(ContinuousTileSource.this){if(!closed)error="Continuous GPU source: "+failure.getClass().getSimpleName();}}
                finally{synchronized(ContinuousTileSource.this){if(!closed&&error.isEmpty())error="Continuous GPU profiler exited";ContinuousTileSource.this.notifyAll();}}
            },"CyOVR-Continuous-Tile-Reader");reader.start();
        }
        if(!target.equals(requested))throw new IOException("Stop the previous continuous target first");
        long until=System.nanoTime()+TimeUnit.SECONDS.toNanos(3);
        while(!closed&&sequence==delivered&&error.isEmpty()){long remaining=until-System.nanoTime();if(remaining<=0)break;TimeUnit.NANOSECONDS.timedWait(this,remaining);}
        if(closed)throw new IOException("Continuous GPU source stopped");
        if(!error.isEmpty())throw new IOException(error);
        if(sequence!=delivered){delivered=sequence;return latest;}
        return new JSONObject().put("status","unavailable").put("reason","Waiting for a complete target section from the continuous GPU profiler. Check that detailed profiling and both Vulkan layers are active.");
    }
    @Override public void close(){java.lang.Process owned;Thread input;
        synchronized(this){closed=true;owned=process;input=reader;process=null;latest=null;notifyAll();}
        if(owned!=null){owned.destroy();try{if(!owned.waitFor(500,TimeUnit.MILLISECONDS)){owned.destroyForcibly();owned.waitFor(1000,TimeUnit.MILLISECONDS);}}catch(InterruptedException e){owned.destroyForcibly();Thread.currentThread().interrupt();}}
        if(input!=null&&input!=Thread.currentThread())try{input.join(1000);}catch(InterruptedException e){Thread.currentThread().interrupt();}
    }
}

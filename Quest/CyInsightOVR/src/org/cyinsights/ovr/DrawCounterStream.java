// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.os.Process;
import android.system.Os;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.atomic.AtomicBoolean;
import org.json.*;

/** Persistent run-as reader: one process, no per-poll shell/cat process spawning. */
public final class DrawCounterStream {
    public static void main(String[] args)throws Exception{
        if(args.length!=2||!args[0].matches("[A-Za-z][A-Za-z0-9_]*(?:\\.[A-Za-z0-9_]+)+")||Process.myUid()%100000<10000)throw new IllegalArgumentException("Target app identity required");
        int hz=DrawCounterSample.frequency(Integer.parseInt(args[1]));
        File file=new File("/data/user/"+(Process.myUid()/100000)+"/"+args[0]+"/files/cyinsights-draw-counters.json");
        AtomicBoolean connected=new AtomicBoolean(true);
        Thread owner=new Thread(()->{try{while(System.in.read()!=-1){}}catch(IOException ignored){}finally{connected.set(false);}},"CyDraw-Owner");owner.setDaemon(true);owner.start();
        long renewed=0;while(connected.get()){long start=System.nanoTime();
            if(start-renewed>=500000000L){
                File temporary=new File(file+".enabled.tmp");try(FileOutputStream out=new FileOutputStream(temporary)){out.write((Long.toString(start)+" "+hz).getBytes(StandardCharsets.UTF_8));}
                if(!temporary.setReadable(false,false)||!temporary.setWritable(false,false)||!temporary.setReadable(true,true)||!temporary.setWritable(true,true)||!temporary.renameTo(new File(file+".enabled")))throw new IOException("Cannot enable private counter stream");renewed=start;
            }
            try{
                if(Os.lstat(file.toString()).st_uid!=Process.myUid()||file.length()>16384)throw new IOException("Invalid counter-file ownership or size");
                byte[] bytes=new byte[16385];int length;try(InputStream input=new FileInputStream(file)){length=input.read(bytes);if(length<1||length>16384||input.read()!=-1)throw new IOException("Invalid counter payload");}
                JSONObject raw=new JSONObject(new String(bytes,0,length,StandardCharsets.UTF_8));int pid=raw.getInt("pid");
                byte[] name=new byte[256];int n;try(InputStream proc=new FileInputStream("/proc/"+pid+"/cmdline")){n=proc.read(name);}
                int end=0;while(end<n&&name[end]!=0)++end;
                if(!args[0].equals(new String(name,0,end,StandardCharsets.UTF_8)))throw new IOException("Counter source is not the selected running target");
                // Validating here bounds and authenticates the target-local input;
                // rate/coverage validation is repeated by the shell-side consumer.
                System.out.println(raw.put("reader_pid",Process.myPid()).toString());
            }catch(Exception error){System.out.println(new JSONObject().put("source_error","Draw counter source unavailable. Update/prepare the development layer and reopen the target once.").toString());}
            long remaining=1000000000L/hz-(System.nanoTime()-start);if(remaining>0)Thread.sleep(remaining/1000000L,(int)(remaining%1000000L));
        }
    }
}

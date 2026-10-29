// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import java.io.*;import java.nio.charset.StandardCharsets;import org.json.*;
/** Explicit fixtures only. Does not open a profiler, game, screen sharing or HUD. */
public final class TileRecordingTests {
    static void check(boolean value,String message){if(!value)throw new AssertionError(message);}
    static JSONObject read(File file)throws Exception{try(BufferedReader in=new BufferedReader(new FileReader(file))){return new JSONObject(in.readLine());}}
    static void remove(File file){File[] files=file.listFiles();if(files!=null)for(File child:files)remove(child);file.delete();}
    static JSONObject sample(String boot)throws Exception{
        String text="Process org.fixture.game has 1 surface executions\nSurface 1 | 100x80 | color 32bit, MSAA 4 | 2 64x64 bins | 1 ms\nBin 0 | topLeft 0 x0 | 1x1 logical bins\nStage 0 : Render 0.25 ms\n";
        return ProfilerParser.capture(text,"org.fixture.game",boot,Long.toString(System.nanoTime()),true);
    }
    public static void main(String[] args)throws Exception{
        File root=new File("/data/local/tmp/cyovr-recording-test-"+android.os.Process.myPid());
        if(root.exists())throw new IOException("Test directory already exists");
        try{
            long now=System.nanoTime();JSONObject view=new JSONObject().put("active",true).put("android_boot_id","fixture-boot").put("observed_mono_ns",Long.toString(now-10000000)).put("width",10).put("height",10);
            JSONObject a=TileRecording.associate(view,"fixture-boot",now);check(a.getString("status").equals("measured")&&a.getDouble("delta_ms")==-10,"Association lost signed offset");
            check(a.isNull("execution_frame")&&a.getString("association").equals("nearest_observation_approximate"),"Association claimed exact execution");
            check(TileRecording.associate(view,"another-boot",now).getString("status").equals("unavailable"),"Another boot accepted");
            check(TileRecording.associate(view,"fixture-boot",now+2000000000L).getString("status").equals("unavailable"),"Stale image accepted");
            check(TileRecording.associate(view,"fixture-boot",now-2000000000L).getString("status").equals("unavailable"),"Distant future image accepted");
            view.put("active",false);check(TileRecording.associate(view,"fixture-boot",now).getString("status").equals("unavailable"),"Stopped sharing accepted");
            TileRecording recording=new TileRecording(target->new JSONObject().put("active",true).put("android_boot_id","fixture-boot").put("observed_mono_ns",Long.toString(System.nanoTime())).put("width",10).put("height",10).put("jpeg_base64","/9j/2Q=="),"fixture-boot",root);
            JSONObject options=new JSONObject().put("package","org.fixture.game").put("hz",5).put("images",true);
            JSONObject started=recording.start(options);String id=started.getString("recording_id");
            try{recording.start(options);throw new AssertionError("Concurrent recording accepted");}catch(IOException expected){}
            try{recording.stop("wrong-recording");throw new AssertionError("Another recording stopped");}catch(IOException expected){}check(recording.active(),"Wrong stop changed recording");
            JSONObject invalid=sample("fixture-boot");invalid.put("target_package","org.other.game");recording.offer(invalid);Thread.sleep(50);check(recording.status().getInt("samples")==0,"Another target recorded");
            invalid=sample("fixture-boot");invalid.put("observed_mono_ns","1");recording.offer(invalid);Thread.sleep(50);check(recording.status().getInt("samples")==0,"Pre-recording sample accepted");
            recording.offer(sample("fixture-boot"));long deadline=System.nanoTime()+3000000000L;while(recording.status().getInt("samples")==0&&System.nanoTime()<deadline)Thread.sleep(20);
            JSONObject stopped=recording.stop(id);check(stopped.getInt("samples")==1&&stopped.getInt("images")==1&&!stopped.getBoolean("active"),"Measured fixture not finalized");
            File folder=new File(root,id);JSONObject info=read(new File(folder,"info.json")),row=read(new File(folder,"index.jsonl"));check(info.getInt("sample_count")==1&&info.getInt("image_count")==1,"Metadata counts incomplete");
            check(row.getDouble("max_render_us")==250&&row.getInt("tile_count")==1,"Tile costs changed during persistence");
            check(new File(folder,row.getString("path")).isFile()&&new File(folder,row.getJSONObject("image").getString("path")).length()==4,"Payload paths not persisted");
            options.put("images",false);id=recording.start(options).getString("recording_id");recording.offer(sample("fixture-boot"));stopped=recording.stop(id);check(stopped.getInt("samples")==1&&stopped.getInt("images")==0,"Images-disabled recording saved an image");
            id=recording.start(options).getString("recording_id");recording.offer(sample("wrong-boot"));deadline=System.nanoTime()+3000000000L;while(recording.active()&&System.nanoTime()<deadline)Thread.sleep(20);stopped=recording.stop(id);check(stopped.getInt("samples")==0&&!stopped.getString("error").isEmpty(),"Boot change silently attributed tiles");
            System.out.println("17 tile recording lifecycle, image association, opt-in and persistence checks passed (explicit fixtures)");
        }finally{remove(root);}
    }
}

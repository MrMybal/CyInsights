// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.security.*;
import java.util.*;
import org.json.*;

/** Own-UID streaming export in the desktop's existing CYCAP001 container. */
final class RecordingExport {
    private static void integer(OutputStream out,long value,int bytes)throws IOException{for(int i=0;i<bytes;++i)out.write((int)(value>>>(8*i))&255);}
    private static byte[] hash(File file)throws Exception{
        MessageDigest digest=MessageDigest.getInstance("SHA-256");byte[] buffer=new byte[65536];
        try(InputStream in=new FileInputStream(file)){int n;while((n=in.read(buffer))!=-1)digest.update(buffer,0,n);}return digest.digest();
    }
    private static void header(OutputStream out,String name,long size,byte[] hash)throws Exception{
        byte[] path=name.getBytes(StandardCharsets.UTF_8);integer(out,path.length,4);integer(out,size,8);out.write(hash);out.write(path);
    }
    private static void add(List<File> files,File root,String child,String pattern)throws Exception{
        File dir=new File(root,child);if(Files.isSymbolicLink(dir.toPath()))throw new IOException("Recording links are not exportable");
        File[] entries=dir.listFiles();if(entries==null)throw new IOException("Recording directory unavailable");
        Arrays.sort(entries,Comparator.comparing(File::getName));
        for(File file:entries){if(!file.getName().matches(pattern)||!file.isFile()||Files.isSymbolicLink(file.toPath()))throw new IOException("Unexpected recording payload");files.add(file);}
    }
    static void write(File recordings,JSONObject state,String id,OutputStream target)throws Exception{
        if(!id.matches("[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}"))throw new IllegalArgumentException("Invalid recording identity");
        if(state.has("error")&&state.length()==1)throw new IOException("GPU service unavailable for recording export");
        if(state.optBoolean("active")&&id.equals(state.optString("recording_id")))throw new IOException("Stop this recording before exporting it");
        File root=new File(recordings,id);
        if(!root.getAbsoluteFile().equals(root.getCanonicalFile()))throw new IOException("Recording path is not canonical");
        File info=new File(root,"info.json"),index=new File(root,"index.jsonl");
        if(info.length()>1024*1024||!info.isFile()||!index.isFile()||Files.isSymbolicLink(info.toPath())||Files.isSymbolicLink(index.toPath()))throw new IOException("Invalid recording metadata");
        JSONObject metadata=new JSONObject(new String(Files.readAllBytes(info.toPath()),StandardCharsets.UTF_8));
        if(!id.equals(metadata.optString("recording_id"))||!"cyinsights.quest-tiles".equals(metadata.optString("schema"))||metadata.optInt("version")!=1)throw new IOException("Recording metadata identity mismatch");
        List<File> files=new ArrayList<>();files.add(info);files.add(index);add(files,root,"samples","[0-9]{6,}\\.cyovr");int samples=files.size()-2;add(files,root,"images","[0-9]{6,}\\.jpg");
        if(samples!=metadata.optInt("sample_count")||files.size()-2-samples!=metadata.optInt("image_count")||files.size()+1>100000)throw new IOException("Recording is incomplete or exceeds the container entry bound");
        JSONObject source=new JSONObject().put("id","quest.ovr.tiles").put("path","info.json").put("status",samples>0?"measured":"unavailable");
        byte[] manifest=new JSONObject().put("format","cycapture").put("version",1).put("session_id",id).put("sources",new JSONArray().put(source)).toString().getBytes(StandardCharsets.UTF_8);
        BufferedOutputStream out=new BufferedOutputStream(target,65536);out.write("CYCAP001".getBytes(StandardCharsets.US_ASCII));integer(out,files.size()+1,4);
        header(out,"manifest.json",manifest.length,MessageDigest.getInstance("SHA-256").digest(manifest));out.write(manifest);
        byte[] buffer=new byte[65536];
        for(File file:files){long size=file.length();byte[] before=hash(file);String name=root.toPath().relativize(file.toPath()).toString().replace(File.separatorChar,'/');header(out,name,size,before);
            MessageDigest copied=MessageDigest.getInstance("SHA-256");long length=0;
            try(InputStream in=new FileInputStream(file)){int n;while((n=in.read(buffer))!=-1){out.write(buffer,0,n);copied.update(buffer,0,n);length+=n;}}
            if(length!=size||file.length()!=size||!MessageDigest.isEqual(before,copied.digest()))throw new IOException("Recording changed during export");
        }
        out.flush();
    }
}

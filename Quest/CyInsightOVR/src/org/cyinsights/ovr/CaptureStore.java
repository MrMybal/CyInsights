// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;

import android.content.Context;
import android.util.AtomicFile;
import java.io.*;
import java.nio.charset.StandardCharsets;
import org.json.*;

/** Shared by the panel and the background recorder; a stopped panel loses no capture. */
final class CaptureStore {
    static synchronized File save(Context context,JSONObject capture)throws IOException {
        String id=capture.optString("capture_id");
        if(!id.matches("[0-9a-fA-F-]{36}"))throw new IOException("Invalid capture identity");
        File root=context.getExternalFilesDir(null);if(root==null)throw new IOException("Application capture storage is unavailable");
        File directory=new File(root,"Captures");
        if(!directory.isDirectory()&&!directory.mkdirs())throw new IOException("Cannot create capture directory");
        File destination=new File(directory,"capture_"+id+".cyovr");
        AtomicFile file=new AtomicFile(destination);FileOutputStream output=null;
        try{output=file.startWrite();output.write(capture.toString().getBytes(StandardCharsets.UTF_8));file.finishWrite(output);}
        catch(IOException e){if(output!=null)file.failWrite(output);throw e;}
        if(!context.getSharedPreferences("capture",0).edit().putString("last_file",destination.getName()).commit())
            throw new IOException("Capture saved, but its reopening preference could not be persisted");
        return destination;
    }
    static synchronized JSONObject latest(Context context)throws Exception {
        String name=context.getSharedPreferences("capture",0).getString("last_file","");
        if(!name.matches("capture_[0-9a-fA-F-]{36}\\.cyovr"))return null;
        File root=context.getExternalFilesDir(null);if(root==null)return null;
        File directory=new File(root,"Captures");
        AtomicFile file=new AtomicFile(new File(directory,name));
        if(!file.getBaseFile().exists()&&!new File(file.getBaseFile().getPath()+".bak").exists())return null;
        try(InputStream in=file.openRead();ByteArrayOutputStream out=new ByteArrayOutputStream()){
            byte[] buffer=new byte[8192];int n;
            while((n=in.read(buffer))!=-1){if(out.size()+n>8*1024*1024)throw new IOException("Tile snapshot is too large");out.write(buffer,0,n);}
            return new JSONObject(new String(out.toByteArray(),StandardCharsets.UTF_8));
        }
    }
}

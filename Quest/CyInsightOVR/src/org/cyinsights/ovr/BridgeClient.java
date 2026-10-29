// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.content.Context;
import java.net.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import org.json.*;
final class BridgeClient {
    static JSONObject request(Context context,JSONObject req) throws Exception {
        return request(new File(context.getFilesDir(),"developer-bridge.json"),req);
    }
    static JSONObject request(File config,JSONObject req) throws Exception {
        String line;
        try(BufferedReader reader=new BufferedReader(new FileReader(config))){line=reader.readLine();}
        if(line==null||line.length()>1024)throw new IOException("Invalid developer bridge configuration");
        JSONObject connection=new JSONObject(line);int port=connection.getInt("port");
        String token=connection.getString("token");
        if(port<1024||port>65535||!token.matches("[0-9a-f]{64}"))throw new IOException("Invalid developer bridge configuration");
        req.put("authentication",token);
        try(Socket socket=new Socket()) {
            socket.connect(new InetSocketAddress(InetAddress.getByName("127.0.0.1"),port),2000);
            socket.setSoTimeout((req.optString("operation").startsWith("overlay_meta_")||req.optString("operation").startsWith("heatmap_")||req.optString("operation").startsWith("tiles_record_"))?30000:7000);socket.getOutputStream().write((req.toString()+"\n").getBytes(StandardCharsets.UTF_8));
            InputStream input=socket.getInputStream();ByteArrayOutputStream data=new ByteArrayOutputStream();byte[] b=new byte[8192];int n;
            while((n=input.read(b))>0){if(data.size()+n>8*1024*1024)throw new IOException("Bridge response too large");data.write(b,0,n);if(b[n-1]=='\n')break;}
            return new JSONObject(new String(data.toByteArray(),StandardCharsets.UTF_8));
        }
    }
}

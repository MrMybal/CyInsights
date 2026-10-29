// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.net.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import org.json.*;
public final class ShellClient {
    public static void main(String[] args) throws Exception {
        if(args.length==2&&args[0].equals("--request")){if(args[1].length()>12000)throw new IllegalArgumentException("Request too large");JSONObject request=new JSONObject(new String(android.util.Base64.decode(args[1],android.util.Base64.DEFAULT),StandardCharsets.UTF_8));System.out.println(BridgeClient.request(new File("/data/local/tmp/cyinsightovr-connection.json"),request));return;}
        if(args.length<1||args.length>7)throw new IllegalArgumentException("operation [package] [seconds] [mode] [position] [ceiling_us] [opacity]");
        JSONObject request=new JSONObject();request.put("operation",args[0]);request.put("package",args.length>1?args[1]:"org.cyinsights.ovr");request.put("seconds",args.length>2?Double.parseDouble(args[2]):.3);
        if(args.length>3)request.put("mode",args[3]);if(args.length>4)request.put("position",Integer.parseInt(args[4]));if(args.length>5)request.put("ceiling_us",Double.parseDouble(args[5]));if(args.length>6)request.put("opacity",Double.parseDouble(args[6]));
        System.out.println(BridgeClient.request(new File("/data/local/tmp/cyinsightovr-connection.json"),request).toString());
    }
}

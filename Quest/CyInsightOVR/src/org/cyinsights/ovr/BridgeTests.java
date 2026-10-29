// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import org.json.*;
/** Device integration test: never prints the pairing capability. */
public final class BridgeTests {
    public static void main(String[] args)throws Exception {
        File file=new File("/data/local/tmp/cyinsightovr-connection.json");JSONObject config;
        try(BufferedReader reader=new BufferedReader(new FileReader(file))){config=new JSONObject(reader.readLine());}
        try(Socket socket=new Socket()){
            socket.connect(new InetSocketAddress("127.0.0.1",config.getInt("port")),2000);socket.setSoTimeout(3000);
            socket.getOutputStream().write("{\"operation\":\"snapshot\"}\n".getBytes(StandardCharsets.UTF_8));
            JSONObject rejected=new JSONObject(new BufferedReader(new InputStreamReader(socket.getInputStream(),StandardCharsets.UTF_8)).readLine());
            if(!"Unauthorized bridge request".equals(rejected.optString("error"))||rejected.has("metrics"))throw new AssertionError("Unauthenticated access was not rejected");
        }
        JSONObject valid=BridgeClient.request(file,new JSONObject().put("operation","snapshot"));
        if(!valid.optBoolean("bridge")||valid.has("authentication"))throw new AssertionError("Authenticated recovery / capability isolation failed");
        JSONObject invalid=BridgeClient.request(file,new JSONObject().put("operation","arbitrary_exec"));
        if(!invalid.has("error"))throw new AssertionError("Unknown operation accepted");
        System.out.println("3 device bridge checks passed: unauthorized rejection, authenticated recovery, operation allowlist");
    }
}

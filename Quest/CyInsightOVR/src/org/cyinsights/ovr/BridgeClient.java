// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.content.Context;
import java.net.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import org.json.*;
final class BridgeClient {
    enum Failure { MISSING, INVALID, REBOOT, STOPPED, TIMEOUT, REJECTED, RESPONSE }
    static final class Unavailable extends IOException {
        final Failure failure;
        Unavailable(Failure failure){super(description(failure));this.failure=failure;}
    }
    private static final String SETUP="Open CyInsightOVR on the Quest, then Connection > Start GPU service. The service runs in this app; no PC bootstrap is needed.";
    private static String description(Failure failure){
        switch(failure){
            case MISSING:return "The GPU service has not published its local connection yet. "+SETUP;
            case INVALID:return "The GPU service connection needs to be refreshed. "+SETUP;
            case REBOOT:return "This GPU connection belongs to the previous headset boot. "+SETUP;
            case REJECTED:return "The GPU service restarted and the old connection expired. "+SETUP;
            case TIMEOUT:return "The GPU service is not responding. Retry once, or stop/start the service in CyInsightOVR > Connection.";
            case RESPONSE:return "The GPU service returned an invalid response. Stop/start it in CyInsightOVR > Connection.";
            default:return "The GPU service is stopped or unavailable. "+SETUP;
        }
    }
    static String failureMessage(Exception error){return error instanceof Unavailable||error instanceof DirectMetaHud.SetupRequired?error.getMessage():"Quest request failed: "+(error.getMessage()==null?error.getClass().getSimpleName():error.getMessage());}
    private static String bootId(){
        try(BufferedReader reader=new BufferedReader(new FileReader("/proc/sys/kernel/random/boot_id"))){String value=reader.readLine();return value==null?"":value.trim();}
        catch(IOException|SecurityException unavailable){return "";}
    }
    private static final String BOOT=bootId();
    static JSONObject request(Context context,JSONObject req) throws Exception {
        String operation=req.optString("operation");boolean standalone=DirectMetaHud.available(context);
        if(operation.equals("display_hud_on")||operation.equals("display_hud_off"))return DisplayHudController.apply(req,options->request(context,options));
        if(operation.equals("graph_hud_stop")&&!GpuDataService.running())return new JSONObject().put("active",false);
        if(standalone&&DirectMetaHud.handles(operation))return DirectMetaHud.request(context,req);
        if(operation.equals("snapshot")&&!GpuDataService.enabled(context))return (standalone?DirectMetaHud.snapshot(context):new JSONObject().put("bridge",false)).put("gpu_service_stopped",true).put("status","GPU service stopped. Start it in Connection to use GPU curves and the tile heatmap.");
        GpuDataService.require(context);
        try{JSONObject response=request(new File(context.getFilesDir(),"developer-bridge.json"),req);if(operation.equals("snapshot"))response.put("standalone_hud",standalone);return response;}
        catch(Unavailable offline){
            if(operation.equals("snapshot")){if(standalone)return DirectMetaHud.snapshot(context);throw new DirectMetaHud.SetupRequired();}
            if(DirectMetaHud.handles(operation))throw new DirectMetaHud.SetupRequired();
            throw offline;
        }
    }
    static JSONObject request(File config,JSONObject req) throws Exception {
        return request(config,req,BOOT);
    }
    static void exportRecording(File config,String id,OutputStream output)throws Exception{
        if(!id.matches("[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}"))throw new IllegalArgumentException("Invalid recording identity");
        if(!config.isFile()||config.length()>2048)throw new Unavailable(Failure.MISSING);
        JSONObject connection;try(BufferedReader reader=new BufferedReader(new FileReader(config))){connection=new JSONObject(reader.readLine());}
        int port=connection.getInt("port");String token=connection.getString("token");
        if(port<1024||port>65535||!token.matches("[0-9a-f]{64}"))throw new Unavailable(Failure.INVALID);
        if(!BOOT.isEmpty()&&!BOOT.equals(connection.optString("android_boot_id",BOOT)))throw new Unavailable(Failure.REBOOT);
        try(Socket socket=new Socket()){
            socket.connect(new InetSocketAddress("127.0.0.1",port),2000);socket.setSoTimeout(30000);
            JSONObject request=new JSONObject().put("operation","recording_export").put("recording_id",id).put("authentication",token);
            socket.getOutputStream().write((request.toString()+"\n").getBytes(StandardCharsets.UTF_8));
            DataInputStream input=new DataInputStream(socket.getInputStream());byte[] magic=new byte[8];input.readFully(magic);
            if(!"CYCAP001".equals(new String(magic,StandardCharsets.US_ASCII)))throw new IOException("GPU service did not return a native recording container");
            output.write(magic);byte[] buffer=new byte[65536];int n;while((n=input.read(buffer))!=-1)output.write(buffer,0,n);output.flush();
        }
    }
    static JSONObject request(File config,JSONObject req,String currentBoot) throws Exception {
        if(!config.isFile())throw new Unavailable(Failure.MISSING);
        int port;String token;
        try{
            if(config.length()>2048)throw new Unavailable(Failure.INVALID);
            String line;try(BufferedReader reader=new BufferedReader(new FileReader(config))){line=reader.readLine();}
            if(line==null||line.length()>1024)throw new Unavailable(Failure.INVALID);
            JSONObject connection=new JSONObject(line);port=connection.getInt("port");token=connection.getString("token");
            if(port<1024||port>65535||!token.matches("[0-9a-f]{64}"))throw new Unavailable(Failure.INVALID);
            String previousBoot=connection.optString("android_boot_id");
            if(!currentBoot.isEmpty()&&!previousBoot.isEmpty()&&!currentBoot.equals(previousBoot))throw new Unavailable(Failure.REBOOT);
        }catch(Unavailable known){throw known;}catch(IOException|JSONException invalid){throw new Unavailable(Failure.INVALID);}
        JSONObject wire=new JSONObject(req.toString()).put("authentication",token);
        try(Socket socket=new Socket()) {
            socket.connect(new InetSocketAddress(InetAddress.getByName("127.0.0.1"),port),2000);
            socket.setSoTimeout((req.optString("operation").startsWith("display_hud_")||req.optString("operation").startsWith("overlay_meta_")||req.optString("operation").startsWith("heatmap_")||req.optString("operation").startsWith("tiles_record_"))?30000:7000);socket.getOutputStream().write((wire.toString()+"\n").getBytes(StandardCharsets.UTF_8));
            InputStream input=socket.getInputStream();ByteArrayOutputStream data=new ByteArrayOutputStream();byte[] b=new byte[8192];int n;
            while((n=input.read(b))>0){if(data.size()+n>8*1024*1024)throw new Unavailable(Failure.RESPONSE);data.write(b,0,n);if(b[n-1]=='\n')break;}
            JSONObject response=new JSONObject(new String(data.toByteArray(),StandardCharsets.UTF_8));
            if("Unauthorized bridge request".equals(response.optString("error")))throw new Unavailable(Failure.REJECTED);
            return response;
        }catch(Unavailable known){throw known;}catch(SocketTimeoutException timeout){throw new Unavailable(Failure.TIMEOUT);}
        catch(IOException offline){throw new Unavailable(Failure.STOPPED);}catch(JSONException invalid){throw new Unavailable(Failure.RESPONSE);}
    }
}

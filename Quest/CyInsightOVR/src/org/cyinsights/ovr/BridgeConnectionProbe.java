// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.app.*;
import android.os.Bundle;
import org.json.*;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.concurrent.atomic.AtomicReference;

/** Isolated connection failures and one read-only request to the real collector. */
public final class BridgeConnectionProbe extends Instrumentation {
    private int checks;
    private static final String TOKEN;
    static{char[] fake=new char[64];Arrays.fill(fake,'a');TOKEN=new String(fake);}
    private void check(boolean value,String why){++checks;if(!value)throw new AssertionError(why);}
    private void save(File file,int port,String boot)throws Exception{
        JSONObject j=new JSONObject().put("port",port).put("token",TOKEN).put("android_boot_id",boot);
        try(OutputStream out=new FileOutputStream(file)){out.write(j.toString().getBytes(StandardCharsets.UTF_8));}
    }
    private void failure(File file,BridgeClient.Failure expected)throws Exception{
        try{BridgeClient.request(file,new JSONObject().put("operation","snapshot"),"test-current-boot");throw new AssertionError("Connection failure was not detected");}
        catch(BridgeClient.Unavailable e){check(e.failure==expected,"Incorrect recovery reason");check(!e.getMessage().contains(TOKEN),"Connection error exposes authentication");}
    }
    private void server(File file,String reply,BridgeClient.Failure expected)throws Exception{
        AtomicReference<Throwable> error=new AtomicReference<>();
        try(ServerSocket listener=new ServerSocket(0,1,InetAddress.getByName("127.0.0.1"))){
            listener.setSoTimeout(5000);save(file,listener.getLocalPort(),"test-current-boot");
            Thread service=new Thread(()->{
                try(Socket socket=listener.accept()){
                    socket.setSoTimeout(5000);BufferedReader input=new BufferedReader(new InputStreamReader(socket.getInputStream(),StandardCharsets.UTF_8));
                    JSONObject wire=new JSONObject(input.readLine());
                    if(!TOKEN.equals(wire.optString("authentication")))throw new AssertionError("Wire authentication missing");
                    socket.getOutputStream().write((reply+"\n").getBytes(StandardCharsets.UTF_8));
                }catch(Throwable e){error.set(e);}
            },"CyOVR-Connection-Fixture");service.setDaemon(true);service.start();
            try{
                if(expected!=null)failure(file,expected);
                else {JSONObject request=new JSONObject().put("operation","snapshot");JSONObject response=BridgeClient.request(file,request,"test-current-boot");check(response.optBoolean("bridge"),"Valid helper response rejected");check(!request.has("authentication"),"Caller request retains authentication");}
            }finally{listener.close();service.join(6000);}
            check(!service.isAlive()&&error.get()==null,"Fixture server failed");
        }
    }
    @Override public void onCreate(Bundle args){super.onCreate(args);start();}
    @Override public void onStart(){Bundle out=new Bundle();File file=null;
        try{
            file=File.createTempFile("collector-connection-",".json",getTargetContext().getCacheDir());
            check(file.delete(),"Cannot prepare missing-config test");failure(file,BridgeClient.Failure.MISSING);
            try(OutputStream stream=new FileOutputStream(file)){stream.write("invalid".getBytes(StandardCharsets.UTF_8));}failure(file,BridgeClient.Failure.INVALID);
            save(file,12345,"test-previous-boot");failure(file,BridgeClient.Failure.REBOOT);
            boolean bootReadable=false;try(BufferedReader boot=new BufferedReader(new FileReader("/proc/sys/kernel/random/boot_id"))){String value=boot.readLine();bootReadable=value!=null&&!value.isEmpty();}catch(IOException unavailable){}
            out.putBoolean("headset_boot_readable",bootReadable);
            if(bootReadable){try{BridgeClient.request(file,new JSONObject().put("operation","snapshot"));throw new AssertionError("Real boot mismatch was not detected");}catch(BridgeClient.Unavailable e){check(e.failure==BridgeClient.Failure.REBOOT,"Real Android boot mismatch diagnosis failed");}}
            int closedPort;try(ServerSocket reserve=new ServerSocket(0,1,InetAddress.getByName("127.0.0.1"))){closedPort=reserve.getLocalPort();}
            save(file,closedPort,"test-current-boot");failure(file,BridgeClient.Failure.STOPPED);
            server(file,"{\"error\":\"Unauthorized bridge request\"}",BridgeClient.Failure.REJECTED);
            server(file,"invalid JSON",BridgeClient.Failure.RESPONSE);
            server(file,"{\"bridge\":true}",null);
            JSONObject real=BridgeClient.request(getTargetContext(),new JSONObject().put("operation","snapshot"));check(real.optBoolean("bridge"),"Real collector is disconnected");
            out.putBoolean("real_collector_connected",true);out.putBoolean("user_configuration_unchanged",true);out.putInt("checks",checks);out.putString("result","passed");finish(Activity.RESULT_OK,out);
        }catch(Throwable e){out.putString("error",e.toString());out.putInt("checks",checks);finish(Activity.RESULT_CANCELED,out);}
        finally{if(file!=null)file.delete();}
    }
}

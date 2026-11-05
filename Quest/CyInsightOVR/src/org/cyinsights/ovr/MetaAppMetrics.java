// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.content.*;
import android.os.*;
import java.io.*;
import java.util.concurrent.*;

/** Independent client for the public OVR Metrics 2.x application-metrics IPC.
 * Only functional interface identifiers/parcel layouts are used; no Meta SDK
 * implementation or binary is copied or redistributed. Always uses our real
 * package/UID, never the observed game's identity. */
final class MetaAppMetrics implements AutoCloseable {
    private static final String SERVICE="com.oculus.ovrmonitormetricsservice";
    private static final String INTERFACE="com.oculus.metrics.OVRMonitorMetricsServiceInterface";
    private final Context context;private volatile IBinder remote;private boolean bound;private int generation;
    private CountDownLatch connected=new CountDownLatch(1);
    private final ServiceConnection connection=new ServiceConnection(){
        public void onServiceConnected(ComponentName name,IBinder binder){remote=binder;connected.countDown();}
        public void onServiceDisconnected(ComponentName name){remote=null;}
        public void onBindingDied(ComponentName name){remote=null;}
        public void onNullBinding(ComponentName name){connected.countDown();}
    };
    MetaAppMetrics(Context context){this.context=context.getApplicationContext();}
    synchronized void connect()throws Exception {
        if(remote!=null&&remote.isBinderAlive())return;
        if(Looper.myLooper()==Looper.getMainLooper())throw new IOException("HUD binding must run off the UI thread");
        if(bound){context.unbindService(connection);bound=false;}connected=new CountDownLatch(1);
        bound=context.bindService(new Intent().setComponent(new ComponentName(SERVICE,SERVICE+".MetricsService")),connection,Context.BIND_AUTO_CREATE);
        if(!bound||!connected.await(3,TimeUnit.SECONDS)||remote==null)throw new IOException("Meta custom-metric HUD service did not connect");
        if(!INTERFACE.equals(remote.getInterfaceDescriptor()))throw new IOException("Unsupported Meta metrics interface");
        exchange(14,null); // Public onAppConnected(package, activity).
        ++generation;
    }
    synchronized int generation(){return generation;}
    private Object exchange(int operation,String json)throws Exception {
        IBinder binder=remote;if(binder==null)throw new IOException("Meta custom-metric HUD disconnected");
        Parcel request=Parcel.obtain(),reply=Parcel.obtain();
        try{
            request.writeInterfaceToken(INTERFACE);request.writeString(context.getPackageName());
            if(operation==14)request.writeString(null);
            else if(operation==8)request.writeString(json); // defineAppMetrics
            else if(operation==11){request.writeString(null);request.writeLong(System.currentTimeMillis());request.writeString(json);} // updateAppMetrics2
            else if(operation!=7)throw new IllegalArgumentException("Unsupported public metric operation");
            if(!binder.transact(operation,request,reply,0))throw new IOException("Installed Meta service lacks custom-metric support");
            reply.readException();return operation==14?Boolean.TRUE:operation==7?reply.readString():reply.readInt()!=0;
        }finally{request.recycle();reply.recycle();}
    }
    boolean define(String json)throws Exception {connect();return Boolean.TRUE.equals(exchange(8,json));}
    boolean submit(String json)throws Exception {connect();return Boolean.TRUE.equals(exchange(11,json));}
    String latest()throws Exception {connect();return (String)exchange(7,null);}
    @Override public synchronized void close(){if(bound){try{context.unbindService(connection);}catch(IllegalArgumentException ignored){}bound=false;}remote=null;}
}

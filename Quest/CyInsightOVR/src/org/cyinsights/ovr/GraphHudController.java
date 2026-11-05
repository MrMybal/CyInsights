// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import java.io.*;
import java.nio.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import org.json.*;

/** Measured histories for the app's Vulkan HUD. No synthetic frames or Meta injections. */
final class GraphHudController {
    static final int POINTS=56, MAX_GRAPHS=64;
    private static final class Point {long time;double value;boolean complete;Point(long t,double v,boolean c){time=t;value=v;complete=c;}}
    private static final class Spec {String name,title,unit;double scale,graphMax;boolean graph,value;}
    private final Map<String,ArrayDeque<Point>> history=new HashMap<>();
    private final ArrayList<Spec> selection=new ArrayList<>();
    private String target="";private int position=8,size;private double opacity=.8;private long revision,packetRevision=-1,packetTime;
    private byte[] cached=new byte[0];
    synchronized void sample(JSONObject sample)throws Exception {
        String name=sample.getString("name");long time=Long.parseLong(sample.getString("observed_mono_ns"));
        double value=sample.optDouble("value",Double.NaN);ArrayDeque<Point> points=history.get(name);
        if(points==null){if(history.size()>=128)return;points=new ArrayDeque<>();history.put(name,points);}
        if(!points.isEmpty()&&time<=points.getLast().time)return;
        points.addLast(new Point(time,value,sample.optBoolean("complete",true)));while(points.size()>2400||!points.isEmpty()&&time-points.getFirst().time>120000000000L)points.removeFirst();++revision;
    }
    synchronized void clearDrawHistory(){history.keySet().removeIf(name->name.startsWith("Vulkan "));++revision;}
    private static ArrayList<Spec> selection(JSONObject request)throws Exception {
        JSONArray items=request.getJSONArray("custom_graphs");if(items.length()==0||items.length()>MAX_GRAPHS)throw new IllegalArgumentException("Choose at least one curve");
        ArrayList<Spec> next=new ArrayList<>();Set<String> names=new HashSet<>();
        for(int i=0;i<items.length();++i){JSONObject item=items.getJSONObject(i);Spec spec=new Spec();spec.name=item.getString("name");spec.title=item.getString("title");spec.unit=item.getString("unit");spec.scale=item.getDouble("scale");
            spec.graph=item.optBoolean("graph",true);spec.value=item.optBoolean("value",true);
            spec.graphMax=item.optDouble("graph_max",1000);
            if(spec.name.length()>120||spec.name.isEmpty()||!names.add(spec.name)||spec.title.length()>64||spec.unit.length()>24||!Double.isFinite(spec.scale)||spec.scale<=0||spec.scale>1e12||!Double.isFinite(spec.graphMax)||spec.graphMax<=0||spec.graphMax>1e30||!spec.graph&&!spec.value)throw new IllegalArgumentException("Invalid HUD metric definition");next.add(spec);}
        int place=request.optInt("custom_position",8),s=request.optInt("hud_size",0);double alpha=request.optDouble("custom_opacity",.8);
        if(place<0||place>8||s<0||s>1||!Double.isFinite(alpha)||alpha<.1||alpha>1)throw new IllegalArgumentException("Invalid curve placement");
        if(!request.getString("package").matches("[A-Za-z][A-Za-z0-9_]*(?:\\.[A-Za-z0-9_]+)+"))throw new IllegalArgumentException("Select the target game first");
        return next;
    }
    static void validate(JSONObject request)throws Exception {selection(request);}
    synchronized JSONObject start(JSONObject request)throws Exception {
        ArrayList<Spec> next=selection(request);String selected=request.getString("package");if(!target.equals(selected))clearDrawHistory();
        target=selected;position=request.optInt("custom_position",8);size=request.optInt("hud_size",0);opacity=request.optDouble("custom_opacity",.8);
        selection.clear();selection.addAll(next);++revision;return status();
    }
    synchronized JSONObject stop()throws Exception {selection.clear();cached=new byte[0];++revision;return status();}
    synchronized JSONObject status()throws Exception {return status(System.nanoTime());}
    synchronized JSONObject status(long now)throws Exception {
        JSONArray names=new JSONArray(),values=new JSONArray(),metrics=new JSONArray();int measured=0;
        for(Spec spec:selection){if(spec.graph)names.put(spec.name);if(spec.value)values.put(spec.name);ArrayDeque<Point> historyPoints=history.get(spec.name);Point last=historyPoints==null?null:historyPoints.peekLast();
            boolean fresh=last!=null&&last.time<=now&&now-last.time<3000000000L,valid=fresh&&Double.isFinite(last.value*spec.scale)&&last.value>=0&&last.value*spec.scale<=Float.MAX_VALUE;
            if(valid)++measured;String why=valid?(last.complete?"Measured":"Measured partial total"):fresh?(spec.name.endsWith("/Presented Frame")?"Unavailable: no Vulkan presentation boundary on this XR path":"Source unavailable"):last==null?"Waiting for game measurements":"Source is stale; resume the game";
            metrics.put(new JSONObject().put("name",spec.name).put("title",spec.title).put("graph",spec.graph).put("text",spec.value).put("status",valid?"measured":"unavailable").put("reason",why).put("value",valid?last.value*spec.scale:JSONObject.NULL).put("complete",valid&&last.complete).put("observed_mono_ns",last==null?"0":Long.toString(last.time)).put("graph_max",spec.graphMax).put("unit",spec.unit));
        }
        return new JSONObject().put("active",!selection.isEmpty()).put("target",target).put("graphs",names).put("values",values).put("metrics",metrics).put("measured_count",measured).put("reason",selection.isEmpty()?"HUD stopped":measured>0?"Fresh measured values available":"HUD armed. Resume the selected game; reopen it once after installing or updating the Vulkan layer.").put("backend","meta.custom_app_metrics");
    }
    private static void text(ByteBuffer buffer,String value,int length){byte[] bytes=value.toUpperCase(Locale.ROOT).replaceAll("[^A-Z0-9 .%/+:_-]","?").getBytes(StandardCharsets.US_ASCII);int n=Math.min(bytes.length,length-1);buffer.put(bytes,0,n);for(int i=n;i<length;++i)buffer.put((byte)0);}
    synchronized byte[] packet(String selected,long now) {
        if(selection.isEmpty()||!target.equals(selected))return new byte[0];
        if(packetRevision==revision&&now-packetTime<100000000L)return cached;
        ByteBuffer out=ByteBuffer.allocate(32+selection.size()*(68+POINTS*4)).order(ByteOrder.LITTLE_ENDIAN);
        out.putInt(0x43594731).putInt(2).putInt(position).putInt(size).putFloat((float)opacity).putLong(now).putInt(selection.size());
        for(Spec spec:selection){ArrayDeque<Point> points=history.get(spec.name);Point last=points==null?null:points.peekLast();boolean fresh=last!=null&&last.time<=now&&now-last.time<3000000000L;
            double maximum=1;float[] values=new float[POINTS];Arrays.fill(values,-1);long begin=now-10000000000L;
            if(points!=null){Iterator<Point> iterator=points.iterator();Point current=null,next=iterator.hasNext()?iterator.next():null;
                for(int i=0;i<POINTS;++i){long t=begin+(10000000000L*i)/(POINTS-1);double peak=Double.NaN;
                    while(next!=null&&next.time<=t){current=next;if(next.time>=begin&&Double.isFinite(current.value)&&current.value>=0)peak=Double.isFinite(peak)?Math.max(peak,current.value):current.value;next=iterator.hasNext()?iterator.next():null;}
                    double v=current!=null&&Double.isFinite(current.value)?(Double.isFinite(peak)?peak:current.value):Double.NaN;
                    if(current!=null&&t-current.time<3000000000L&&Double.isFinite(v)&&v>=0&&Double.isFinite(v*spec.scale)&&v*spec.scale<=Float.MAX_VALUE){values[i]=(float)(v*spec.scale);maximum=Math.max(maximum,values[i]);}}

            }
            out.putInt((spec.graph?1:0)|(spec.value?2:0));text(out,spec.title,32);text(out,fresh&&Double.isFinite(last.value*spec.scale)&&last.value>=0&&last.value*spec.scale<=Float.MAX_VALUE?String.format(Locale.ROOT,"%s%.1f %s",last.complete?"":"PARTIAL ",last.value*spec.scale,spec.unit)+(spec.graph?String.format(Locale.ROOT," MAX %.1f",maximum):""):"UNAVAILABLE",32);
            for(float value:values)out.putFloat(Float.isFinite(value)&&value>=0?(float)(value/maximum):-1);
        }
        packetRevision=revision;packetTime=now;cached=out.array();return cached;
    }
}

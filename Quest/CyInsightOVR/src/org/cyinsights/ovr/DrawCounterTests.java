// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import org.json.*;
public final class DrawCounterTests {
    private static int checks=0;
    private static void check(boolean good,String why){++checks;if(!good)throw new AssertionError(why);}
    private static JSONObject raw(long time,long draws,long triangles)throws Exception{
        JSONObject value=new JSONObject().put("version",1).put("pid",123).put("session","100").put("observed_mono_ns",Long.toString(time));
        for(String name:DrawCounterSample.COUNTERS)value.put(name,0);
        return value.put("draws",draws).put("triangles",triangles);
    }
    private static void reject(JSONObject value,long now)throws Exception{
        boolean rejected=false;try{new DrawCounterSample().accept(value,now);}catch(Exception expected){rejected=true;}check(rejected,"Malformed/stale input must be rejected");
    }
    public static void main(String[] args)throws Exception{
        for(int hz:new int[]{1,5,10,20})check(DrawCounterSample.frequency(hz)==hz,"Allowed cadence");
        try{DrawCounterSample.frequency(60);throw new AssertionError("Unsupported cadence");}catch(IllegalArgumentException expected){++checks;}
        DrawCounterSample p=new DrawCounterSample();
        check("waiting".equals(p.accept(raw(1000000000L,10,30),1000000000L).getString("status")),"First observation is not a zero rate");
        JSONObject rate=p.accept(raw(1100000000L,30,330),1100000000L);
        check(rate.getBoolean("draws_complete")&&rate.getBoolean("triangles_complete"),"Known geometry coverage");
        check(rate.getJSONArray("metrics").getJSONObject(0).getDouble("value")==200,"Ten-Hz measured draw rate");
        check(rate.getJSONArray("metrics").getJSONObject(1).getDouble("value")==3000,"Measured triangles rate");
        check(!rate.getBoolean("per_presented_frame_available")&&rate.getJSONArray("metrics").getJSONObject(3).isNull("value"),"No fake XR frames");
        check("unchanged".equals(p.accept(raw(1100000000L,30,330),1100000000L).getString("status")),"Duplicate observations do not add curve samples");
        JSONObject partial=raw(1200000000L,40,360).put("indirect",1).put("unknown_triangles",4).put("unknown_draws",1);
        rate=p.accept(partial,1200000000L);check(!rate.getBoolean("draws_complete")&&!rate.getBoolean("triangles_complete"),"Unknown indirect count stays partial");
        p=new DrawCounterSample();p.accept(raw(1000000000L,0,0),1000000000L);
        JSONObject presented=raw(1100000000L,20,200).put("presented_frames",2).put("frame_draws",18).put("frame_triangles",180);
        rate=p.accept(presented,1100000000L);check(rate.getBoolean("per_presented_frame_available")&&rate.getJSONArray("metrics").getJSONObject(3).getDouble("value")==9,"Only completed presentation intervals counted");
        check(rate.getJSONArray("metrics").getJSONObject(4).getDouble("value")==90,"Presented-frame triangle mean");
        rate=p.accept(raw(1200000000L,2,6).put("session","200"),1200000000L);check("waiting".equals(rate.getString("status")),"Restart establishes a new baseline");
        reject(raw(100,0,0).put("overflow",true),100);
        reject(raw(100,0,0).put("draws",-1),100);
        reject(raw(100,0,0).put("triangles","NaN"),100);
        reject(raw(101,0,0),100);
        reject(raw(100,0,0),3000000101L);
        p=new DrawCounterSample();p.accept(raw(1000000000L,5,6),1000000000L);
        boolean backwards=false;try{p.accept(raw(1100000000L,4,6),1100000000L);}catch(IllegalArgumentException expected){backwards=true;}check(backwards,"Counter rollback is not a negative rate");
        System.out.println(checks+" draw-counter cadence, coverage and identity checks passed (synthetic fixtures)");
    }
}

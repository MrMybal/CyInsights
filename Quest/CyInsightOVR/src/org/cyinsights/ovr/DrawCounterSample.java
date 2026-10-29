// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import org.json.*;

/** Cumulative submission counters -> measured windows, never fabricated frames. */
final class DrawCounterSample {
    static final String[] COUNTERS={"submissions","draws","triangles","indirect","unknown_triangles","unknown_draws","missing_buffers","presented_frames","frame_draws","frame_triangles","frame_unknown_draws","frame_unknown_triangles","frame_missing_buffers"};
    static int frequency(int hz){if(hz!=1&&hz!=5&&hz!=10&&hz!=20)throw new IllegalArgumentException("Curve frequency must be 1, 5, 10 or 20 Hz");return hz;}
    static long number(JSONObject value,String key)throws Exception{
        String text=value.get(key).toString();if(!text.matches("[0-9]{1,19}"))throw new IllegalArgumentException("Invalid counter: "+key);
        return Long.parseLong(text);
    }
    private JSONObject previous;
    private static void metric(JSONArray out,String name,Object value,long stamp,boolean complete)throws Exception{
        out.put(new JSONObject().put("name",name).put("value",value).put("observed_mono_ns",Long.toString(stamp)).put("source","vulkan_submissions").put("complete",complete));
    }
    JSONObject accept(JSONObject raw,long now)throws Exception{
        if(raw.getInt("version")!=1||raw.getInt("pid")<=0||raw.optBoolean("overflow"))throw new IllegalArgumentException("Unsupported or overflowing draw counters");
        long stamp=number(raw,"observed_mono_ns");number(raw,"session");
        if(stamp>now||now-stamp>2000000000L)throw new IllegalArgumentException("Draw counters are stale or from a different boot");
        for(String key:COUNTERS)number(raw,key);
        JSONObject out=new JSONObject().put("status","waiting").put("session",raw.getString("session")).put("pid",raw.getInt("pid"))
            .put("observed_mono_ns",Long.toString(stamp)).put("metrics",new JSONArray()).put("frame_basis","vkQueuePresentKHR; unavailable for XR paths without Vulkan presentation");
        if(previous==null||!raw.getString("session").equals(previous.getString("session"))||raw.getInt("pid")!=previous.getInt("pid")){
            previous=raw;return out.put("reason","Waiting for a second fresh submission-counter sample");
        }
        long before=number(previous,"observed_mono_ns");if(stamp<before)throw new IllegalArgumentException("Source observation clock moved backwards");
        if(stamp==before)return out.put("status","unchanged").put("reason","Waiting for a new source sample");
        long[] delta=new long[COUNTERS.length];for(int i=0;i<delta.length;++i){delta[i]=number(raw,COUNTERS[i])-number(previous,COUNTERS[i]);if(delta[i]<0)throw new IllegalArgumentException("Submission counters moved backwards");}
        previous=raw;if(stamp-before>3000000000L)return out.put("reason","Source gap; establishing a fresh measurement window");
        double seconds=(stamp-before)/1e9;boolean draws=delta[5]==0&&delta[6]==0,triangles=draws&&delta[4]==0;
        JSONArray metrics=out.getJSONArray("metrics");
        metric(metrics,"Vulkan Known Draw Calls/s",delta[1]/seconds,stamp,draws);
        metric(metrics,"Vulkan Known Input Triangles/s",delta[2]/seconds,stamp,triangles);
        metric(metrics,"Vulkan Indirect Commands/s",delta[3]/seconds,stamp,delta[6]==0);
        boolean frames=delta[7]>0&&raw.optInt("devices",1)==1;
        metric(metrics,"Vulkan Draw Calls/Presented Frame",frames?delta[8]/(double)delta[7]:JSONObject.NULL,stamp,frames&&delta[10]==0&&delta[12]==0);
        metric(metrics,"Vulkan Known Triangles/Presented Frame",frames?delta[9]/(double)delta[7]:JSONObject.NULL,stamp,frames&&delta[10]==0&&delta[11]==0&&delta[12]==0);
        return out.put("status","measured").put("interval_seconds",seconds).put("source_hz",1/seconds).put("draws_complete",draws).put("triangles_complete",triangles)
            .put("draws",delta[1]).put("known_input_triangles",delta[2]).put("indirect_commands",delta[3]).put("unknown_triangle_draws",delta[4]).put("unknown_draw_count_commands",delta[5])
            .put("untracked_command_buffers",delta[6]).put("submissions",delta[0]).put("presented_frames",delta[7]).put("per_presented_frame_available",frames)
            .put("reason",triangles?"Measured submitted input geometry; no visible-triangle, eye or shader-output attribution":"Partial geometry: indirect, restart, unknown topology or untracked commands are excluded from known totals");
    }
}

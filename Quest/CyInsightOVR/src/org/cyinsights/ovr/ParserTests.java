// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import org.json.*;
public final class ParserTests {
    public static void main(String[] args) throws Exception {
        String text="Process org.other.app has 1 surface executions\nSurface 1 | 100x80 | color 32bit, depth 24bit, stencil 0 bit, MSAA 4, Mode: 1 | 2 64x64 bins (2 rendered) | 5.08 ms\nBin 0 | topLeft 0 x0 | 1x1 logical bins | Fov 8/8\nStage 0 : Render 1.0 ms\n"+
            "Process org.test.app has 1 surface executions\nSurface 1 | 100x80 | color 32bit, depth 24bit, stencil 0 bit, MSAA 4, Mode: 1 | 2 64x64 bins (2 rendered) | 5.08 ms\nBin 0 | topLeft 0 x0 | 1x1 logical bins | Fov 8/8\nStage 0 : Render 0.0 ms\nStage 1 : Render 0.031 ms\nBin 1 | topLeft 64 x64 | 1x1 logical bins | Fov 4/4\nStage 2 : StoreColor 5.0 us\n";
        JSONObject c=ProfilerParser.capture(text,"org.test.app","boot","100",true);
        JSONArray surfaces=c.getJSONArray("surfaces");if(!c.getString("status").equals("measured")||surfaces.length()!=1)throw new AssertionError("Target filtering");
        JSONArray tiles=surfaces.getJSONObject(0).getJSONArray("tiles");if(tiles.length()!=2||tiles.getJSONObject(1).getInt("width")!=36||tiles.getJSONObject(1).getInt("height")!=16)throw new AssertionError("Edge geometry");
        if(Math.abs(tiles.getJSONObject(0).getJSONObject("stages_us").getDouble("Render")-31)>1e-6)throw new AssertionError("Stage accumulation / units");
        if(tiles.getJSONObject(1).getJSONObject("stages_us").has("Render"))throw new AssertionError("Missing is not zero");
        if(!ProfilerParser.capture(text,"org.missing.app","boot","100",true).getString("status").equals("unavailable"))throw new AssertionError("Unknown target");
        if(!ProfilerParser.capture(text,"org.test.app","boot","100",false).getString("status").equals("unavailable"))throw new AssertionError("Partial capture");
        if(!ProfilerParser.metric("Shader : -1.0",100).isNull("value"))throw new AssertionError("Negative sentinel");
        if(ProfilerParser.metric("Shader : 0.0",100).getDouble("value")!=0)throw new AssertionError("Measured zero");
        String partial=text+"Surface 9 | 100x80 | color 32bit, MSAA 4 | 2 64x64 bins | 1 ms\n";
        JSONObject mixed=ProfilerParser.capture(partial,"org.test.app","boot","100",true);
        if(!"measured".equals(mixed.optString("status"))||mixed.getJSONArray("surfaces").length()!=1||mixed.optInt("surfaces_without_tile_geometry")!=1)throw new AssertionError("Geometry-free surface cannot corrupt measured tiles");
        if(!ProfilerParser.capture("No render stage data was captured during the trace","org.test.app","boot","100",true).getString("reason").contains("headset awake"))throw new AssertionError("Empty source diagnostic");
        JSONObject missing=ProfilerParser.capture(text,"org.missing.app","boot","100",true);
        if(missing.getBoolean("target_process_present")||missing.getJSONArray("observed_processes").length()!=2||!missing.getString("reason").contains("other processes"))throw new AssertionError("Missing target must identify a source attachment failure");
        if(!c.getBoolean("target_process_present")||!c.getBoolean("profiler_output_complete"))throw new AssertionError("Measured source provenance");
        System.out.println("12 Android profiler parser checks passed (explicit synthetic fixtures)");
    }
}

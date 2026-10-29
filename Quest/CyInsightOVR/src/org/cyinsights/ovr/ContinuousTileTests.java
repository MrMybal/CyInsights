// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import java.util.*;import org.json.*;
/** Explicit stream fixtures; no profiler or target process is started. */
public final class ContinuousTileTests {
    public static void main(String[] args)throws Exception{
        ArrayList<JSONObject> received=new ArrayList<>();
        ContinuousTileSource.Sections sections=new ContinuousTileSource.Sections("org.test.app",text->received.add(ProfilerParser.capture(text,"org.test.app","fixture","100",true)));
        String header="Process org.test.app has 1 surface executions";
        String surface="Surface 1 | 100x80 | color 32bit, MSAA 4 | 2 64x64 bins | 1 ms";
        String bin="Bin 0 | topLeft 0 x0 | 1x1 logical bins";
        sections.line("Tracing for 0.1 seconds...");sections.line("Process org.other.app has 0 surface executions");sections.line(header);sections.line(surface);sections.line(bin);sections.line("Stage 0 : Render 0.031 ms");
        if(!received.isEmpty())throw new AssertionError("An incomplete section was published");
        sections.line(header);if(received.size()!=1||!received.get(0).getString("status").equals("measured"))throw new AssertionError("Complete target section lost");
        if(received.get(0).getJSONArray("surfaces").getJSONObject(0).getJSONArray("tiles").getJSONObject(0).getJSONObject("stages_us").getDouble("Render")!=31)throw new AssertionError("Stream cost corrupted");
        sections.line(surface);sections.line(bin);sections.line("Stage 0 : Render 0.02 ms");sections.line("Process org.other.app has 1 surface executions");sections.line(surface);sections.line(bin);sections.line("Stage 0 : Render 99 ms");sections.line(header);
        if(received.size()!=2)throw new AssertionError("Another process was attributed to the target");
        String large=new String(new char[65536]).replace('\0','X');for(int i=0;i<34;++i)sections.line(large);sections.line(header);if(received.size()!=2)throw new AssertionError("An oversized section was published");
        sections.line(surface);sections.line(bin);sections.line("Stage 0 : Render 0.04 ms");sections.line(header);if(received.size()!=3)throw new AssertionError("Stream did not recover after bounded drop");
        sections.line(surface);if(received.size()!=3)throw new AssertionError("Final partial section was published");
        System.out.println("7 continuous tile stream checks passed (explicit synthetic fixtures)");
    }
}

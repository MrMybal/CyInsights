// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
public final class HeatmapTargetTests {
    public static void main(String[] args){
        for(String op:new String[]{"heatmap_start","heatmap_update"}){
            if(!HeatmapTarget.resolve(op,"org.test.old","org.test.running").equals("org.test.running"))throw new AssertionError("Saved capture target redirected the prepared HUD");
            if(!HeatmapTarget.resolve(op,"org.test.old","").equals("org.test.old"))throw new AssertionError("Unprepared selection lost");
        }
        for(String op:new String[]{"heatmap_prepare","capture","overlay_meta_on"})if(!HeatmapTarget.resolve(op,"org.test.selected","org.test.prepared").equals("org.test.selected"))throw new AssertionError("Another operation's selected target changed");
        System.out.println("7 heatmap target selection regression checks passed");
    }
}

// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
/** A saved capture target must not redirect Start/Apply away from the prepared HUD. */
final class HeatmapTarget {
    static String resolve(String operation,String selected,String prepared){
        return (operation.equals("heatmap_start")||operation.equals("heatmap_update"))&&!prepared.isEmpty()?prepared:selected;
    }
}

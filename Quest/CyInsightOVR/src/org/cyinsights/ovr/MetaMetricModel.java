// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import org.json.*;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.*;

/** Own measured values -> documented Meta application-metric JSON schema. */
final class MetaMetricModel {
    private final Map<String,Double> ceilings=new HashMap<>();
    // The installed Quest service reads custom values with JsonReader.nextInt.
    // Use thousands rather than millions for geometry so integer-only HUDs
    // retain useful precision, without changing the application's own data.
    private static double factor(JSONObject metric){return metric.optString("unit").startsWith("M ")?1000:1;}
    private static String unit(JSONObject metric){String unit=metric.optString("unit");return unit.startsWith("M ")?"K "+unit.substring(2):unit;}
    private static boolean available(JSONObject metric){double value=metric.optDouble("value",Double.NaN)*factor(metric);return "measured".equals(metric.optString("status"))&&Double.isFinite(value)&&value>=0&&value<=Integer.MAX_VALUE;}
    static String id(String name)throws Exception {byte[] hash=MessageDigest.getInstance("SHA-256").digest(name.getBytes(StandardCharsets.UTF_8));StringBuilder out=new StringBuilder("cyinsights_");for(int i=0;i<8;++i)out.append(String.format(Locale.ROOT,"%02x",hash[i]&255));return out.toString();}
    JSONObject definitions(JSONObject status)throws Exception {
        JSONArray metrics=new JSONArray(),selected=status.optJSONArray("metrics");
        if(selected!=null)for(int i=0;i<selected.length();++i){JSONObject value=selected.getJSONObject(i);String name=value.getString("name");double top=ceilings.getOrDefault(name,value.optDouble("graph_max",1000)*factor(value));double current=value.optDouble("value",Double.NaN)*factor(value);
            if(Double.isFinite(current)&&current>=0)while(top<current&&top<Integer.MAX_VALUE)top*=2;top=Math.min(Integer.MAX_VALUE,Math.max(1,top));ceilings.put(name,top);
            boolean available=available(value);
            metrics.put(new JSONObject().put("Name",id(name)).put("DisplayName",value.getString("title")+" ("+unit(value)+")"+(available?value.optBoolean("complete",true)?"":" [partial]":" [N/A]"))
                .put("Group","CyInsights").put("RangeMin",0).put("RangeMax",(int)Math.ceil(top)).put("GraphMin",0).put("GraphMax",(int)Math.ceil(top))
                .put("ShowGraph",value.getBoolean("graph")).put("ShowStat",value.getBoolean("text")));
        }
        return new JSONObject().put("Metrics",metrics);
    }
    JSONObject values(JSONObject status)throws Exception {
        JSONObject values=new JSONObject();JSONArray selected=status.optJSONArray("metrics");
        if(selected!=null)for(int i=0;i<selected.length();++i){JSONObject metric=selected.getJSONObject(i);double number=metric.optDouble("value",Double.NaN)*factor(metric);
            if(available(metric))values.put(id(metric.getString("name")),(int)Math.round(number));}
        return values;
    }
}

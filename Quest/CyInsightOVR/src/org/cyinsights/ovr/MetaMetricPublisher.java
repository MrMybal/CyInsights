// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import org.json.*;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.*;

/** Prepares public Meta metrics off the renderer and sends them to the real
 * foreground game's authenticated layer, rather than a companion-only HUD. */
final class MetaMetricPublisher implements AutoCloseable {
    private final AppLayerHost host;private final GraphHudController graphs;private final MetaMetricModel model=new MetaMetricModel();
    private final ScheduledExecutorService worker=Executors.newSingleThreadScheduledExecutor(r->new Thread(r,"CyOVR-Meta-Metrics"));
    private String error="";private boolean armed;private int prepared;
    MetaMetricPublisher(AppLayerHost host){this.host=host;this.graphs=host.graphs;worker.scheduleWithFixedDelay(this::tick,100,100,TimeUnit.MILLISECONDS);}
    synchronized void sync()throws Exception {
        JSONObject status=graphs.status();boolean active=status.optBoolean("active");if(!active&&!armed)return;
        JSONObject next=model.definitions(status),measured=model.values(status);
        long observation=0;JSONArray metrics=status.optJSONArray("metrics");if(metrics!=null)for(int i=0;i<metrics.length();++i)if("measured".equals(metrics.getJSONObject(i).optString("status")))observation=Math.max(observation,metrics.getJSONObject(i).optLong("observed_mono_ns"));
        byte[] payload=new JSONObject().put("target",status.getString("target")).put("definition",next).put("values",measured).put("observation",Long.toString(observation)).toString().getBytes(StandardCharsets.UTF_8);
        host.metaPayload(status.getString("target"),payload);prepared=next.getJSONArray("Metrics").length();armed=active;error="";
    }
    private void tick(){try{sync();}catch(Exception failure){synchronized(this){error=failure.getMessage()==null?failure.getClass().getSimpleName():failure.getMessage();}}}
    synchronized JSONObject status()throws Exception {JSONObject report=host.metaStatus();boolean fresh=report.optBoolean("layer_connected")&&report.optDouble("report_age_ms",Double.MAX_VALUE)<3000;
        report.put("backend","meta.custom_app_metrics").put("same_hud",true).put("prepared_metrics",prepared);
        if(!fresh)report.put("defined_metrics",0).put("last_submitted_metrics",0).put("last_submit_age_ms",JSONObject.NULL);
        if(!error.isEmpty())report.put("error",error);return report;}
    @Override public void close(){worker.shutdownNow();synchronized(this){try{graphs.stop();sync();}catch(Exception ignored){}}}
}

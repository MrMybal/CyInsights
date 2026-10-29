// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
import android.util.Base64;
import org.json.*;
/** Bounded, sampled side recording. One pending sample, one writer, no HUD ownership. */
final class TileRecording {
    interface SharedImage {
        JSONObject read(String target) throws Exception;
    }
    private final SharedImage image;
    private final String boot;
    private final File root;
    private final AtomicReference<JSONObject> pending = new AtomicReference<>();
    private volatile boolean active;
    private volatile String error = "";
    private String id = "", target = "";
    private File directory;
    private Thread writer;
    private long period, next, origin;
    private volatile int count, images;
    private boolean withImages;
    private double hz;
    TileRecording(SharedImage image, String boot) {
        this(image, boot, new File("/data/local/tmp/cyinsightovr-recordings"));
    }
    TileRecording(SharedImage image, String boot, File root) {
        this.image = image;
        this.boot = boot;
        this.root = root;
    }
    synchronized JSONObject start(JSONObject options) throws Exception {
        if (active || writer != null && writer.isAlive())
            throw new IOException("A tile recording is already active");
        target = options.getString("package");
        if (!target.matches("[A-Za-z][A-Za-z0-9_]*(?:\\.[A-Za-z0-9_]+)+"))
            throw new IllegalArgumentException("Invalid target");
        hz = options.optDouble("hz", 1);
        if (!Double.isFinite(hz) || hz < .2 || hz > 5)
            throw new IllegalArgumentException("Tile capture cadence must be 0.2 to 5 Hz");
        withImages = options.optBoolean("images", false);
        id = UUID.randomUUID().toString();
        directory = new File(root, id);
        if (!directory.mkdirs())
            throw new IOException("Cannot create tile recording");
        directory.setReadable(false, false);
        directory.setWritable(false, false);
        directory.setExecutable(false, false);
        directory.setReadable(true, true);
        directory.setWritable(true, true);
        directory.setExecutable(true, true);
        new File(directory, "samples").mkdir();
        new File(directory, "images").mkdir();
        count = images = 0;
        error = "";
        pending.set(null);
        origin = System.nanoTime();
        period = (long)(1e9 / hz);
        next = 0;
        active = true;
        metadata();
        writer = new Thread(this::writeLoop, "CyOVR-Tile-Recording");
        writer.start();
        return status();
    }
    synchronized void offer(JSONObject value) {
        if (!active || !target.equals(value.optString("target_package")) ||
            !"measured".equals(value.optString("status")))
            return;
        long observed;
        try {
            observed = Long.parseLong(value.getString("observed_mono_ns"));
        } catch (Exception invalid) {
            return;
        }
        if (observed < origin)
            return;
        long now = System.nanoTime();
        if (now < next)
            return;
        next = now + period;
        pending.set(value);
    }
    static JSONObject associate(JSONObject view, String boot, long observed) throws Exception {
        if (!view.optBoolean("active") || !boot.equals(view.optString("android_boot_id")))
            return new JSONObject()
                .put("status", "unavailable")
                .put("reason", view.optString("reason","View sharing is stopped or belongs to another headset boot"));
        long stamp = Long.parseLong(view.getString("observed_mono_ns"));
        double delta = (stamp - observed) / 1e6;
        if (Math.abs(delta) > 1000)
            return new JSONObject()
                .put("status", "unavailable")
                .put("reason", "No shared image within one second of the tile observation");
        return new JSONObject()
            .put("status", "measured")
            .put("observed_mono_ns", Long.toString(stamp))
            .put("delta_ms", delta)
            .put("width", view.getInt("width"))
            .put("height", view.getInt("height"))
            .put("association", "nearest_observation_approximate")
            .put("mapping", "composited_view_unregistered")
            .put("source", view.optString("source","mediaprojection"))
            .put("black_frame",view.optBoolean("black_frame",false))
            .put("capture_started_mono_ns",view.optString("capture_started_mono_ns",Long.toString(stamp)))
            .put("capture_finished_mono_ns",view.optString("capture_finished_mono_ns",Long.toString(stamp)))
            .put("execution_frame", JSONObject.NULL);
    }
    private void writeLoop() {
        try (BufferedWriter index = new BufferedWriter(new OutputStreamWriter(
                 new FileOutputStream(new File(directory, "index.jsonl")), StandardCharsets.UTF_8))) {
            while (active || pending.get() != null) {
                JSONObject source = pending.getAndSet(null);
                if (source == null) {
                    Thread.sleep(30);
                    continue;
                }
                JSONObject value = new JSONObject(source.toString());
                long observed = Long.parseLong(value.getString("observed_mono_ns"));
                if (!boot.equals(value.optString("android_boot_id")))
                    throw new IOException("Headset boot changed during recording");
                int sequence = count + 1;
                String sample = String.format(Locale.ROOT, "samples/%06d.cyovr", sequence);
                JSONObject visual = new JSONObject()
                                        .put("status", "unavailable")
                                        .put("reason", withImages ? "No image available" : "Images disabled");
                if (withImages)
                    try {
                        JSONObject view = image.read(target);
                        visual = associate(view, boot, observed);
                        if ("measured".equals(visual.optString("status"))) {
                            byte[] jpeg = Base64.decode(view.getString("jpeg_base64"), Base64.DEFAULT);
                            if (jpeg.length < 4 || jpeg.length > 1024 * 1024 || (jpeg[0] & 255) != 255 ||
                                (jpeg[1] & 255) != 216)
                                throw new IOException("Invalid shared JPEG");
                            String path = String.format(Locale.ROOT, "images/%06d.jpg", sequence);
                            try (FileOutputStream out = new FileOutputStream(new File(directory, path))) {
                                out.write(jpeg);
                            }
                            visual.put("path", path);
                            ++images;
                        }
                    } catch (Exception failure) {
                        visual = new JSONObject()
                                     .put("status", "unavailable")
                                     .put("reason", "Shared image unavailable: " + failure.getClass().getSimpleName());
                    }
                value.put("image", visual);
                try (Writer out = new OutputStreamWriter(new FileOutputStream(new File(directory, sample)),
                                                         StandardCharsets.UTF_8)) {
                    out.write(value.toString());
                }
                double maximum = 0;
                int tiles = 0, renderTiles = 0;
                JSONArray surfaces = value.getJSONArray("surfaces");
                for (int i = 0; i < surfaces.length(); ++i) {
                    JSONArray cells = surfaces.getJSONObject(i).getJSONArray("tiles");
                    for (int j = 0; j < cells.length(); ++j) {
                        JSONObject stages = cells.getJSONObject(j).getJSONObject("stages_us");
                        double render = stages.optDouble("Render", Double.NaN);
                        if (Double.isFinite(render) && render >= 0) {
                            maximum = Math.max(maximum, render);
                            ++renderTiles;
                        }
                        ++tiles;
                    }
                }
                JSONObject row = new JSONObject()
                                     .put("id", sequence)
                                     .put("path", sample)
                                     .put("observed_mono_ns", Long.toString(observed))
                                     .put("relative_seconds", (observed - origin) / 1e9)
                                     .put("max_render_us", renderTiles > 0 ? maximum : JSONObject.NULL)
                                     .put("render_tile_count", renderTiles)
                                     .put("tile_count", tiles)
                                     .put("image", visual);
                index.write(row.toString());
                index.newLine();
                index.flush();
                ++count;
            }
        } catch (Exception failure) {
            error = failure.toString();
            active = false;
        } finally {
            try {
                metadata();
            } catch (Exception ignored) {
            }
        }
    }
    private synchronized void metadata() throws Exception {
        JSONObject info = new JSONObject()
                              .put("schema", "cyinsights.quest-tiles")
                              .put("version", 1)
                              .put("recording_id", id)
                              .put("target_package", target)
                              .put("android_boot_id", boot)
                              .put("origin_mono_ns", Long.toString(origin))
                              .put("clock", "android.monotonic_observation")
                              .put("hz", hz)
                              .put("images_requested", withImages)
                              .put("sample_count", count)
                              .put("image_count", images)
                              .put("error", error);
        File temporary = new File(directory, "info.tmp");
        try (Writer out = new OutputStreamWriter(new FileOutputStream(temporary), StandardCharsets.UTF_8)) {
            out.write(info.toString());
        }
        if (!temporary.renameTo(new File(directory, "info.json")))
            throw new IOException("Cannot publish recording metadata");
    }
    synchronized JSONObject status() throws Exception {
        return new JSONObject()
            .put("active", active)
            .put("recording_id", id)
            .put("target_package", target)
            .put("samples", count)
            .put("images", images)
            .put("error", error)
            .put("directory", id.isEmpty() ? "" : directory.toString());
    }
    JSONObject stop(String requested) throws Exception {
        Thread old;
        synchronized (this) {
            if (!id.equals(requested))
                throw new IOException("Tile recording identity changed");
            active = false;
            old = writer;
        }
        if (old != null)
            old.join(7000);
        if (old != null && old.isAlive())
            throw new IOException("Tile recording is still saving its final sample");
        return status();
    }
    boolean active() { return active; }
}

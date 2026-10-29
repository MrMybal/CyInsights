// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.app.*;
import android.os.*;
import java.io.*;
import org.json.*;
/** Inquires runtime extensions without opening an Activity or creating an XR session. */
public final class CapabilitiesProbe extends Instrumentation {
    @Override public void onCreate(Bundle args){super.onCreate(args);start();}
    @Override public void onStart(){Bundle result=new Bundle();try{
        String text=MainActivity.nativeOpenXrCapabilities(getTargetContext());JSONObject report=new JSONObject(text);
        try(FileOutputStream out=new FileOutputStream(new File(getTargetContext().getFilesDir(),"openxr-capabilities.json"))){out.write(text.getBytes(java.nio.charset.StandardCharsets.UTF_8));}
        result.putString("status",report.optString("status"));result.putString("reason",report.optString("reason"));
        result.putInt("extension_count",report.optInt("extension_count"));
        if(report.isNull("overlay_supported"))result.putString("overlay_support","unknown");else result.putBoolean("overlay_supported",report.optBoolean("overlay_supported"));
        result.putBoolean("created_xr_session",false);finish(Activity.RESULT_OK,result);
    }catch(Exception e){result.putString("error",e.toString());finish(Activity.RESULT_CANCELED,result);}}
}

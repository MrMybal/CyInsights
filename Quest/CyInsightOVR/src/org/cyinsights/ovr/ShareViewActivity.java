// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.app.*;import android.content.*;import android.media.projection.*;import android.os.*;
/** System consent for user-requested view sharing with the desktop profiler. */
public final class ShareViewActivity extends Activity {
    @Override public void onCreate(Bundle state){super.onCreate(state);if(state==null){MediaProjectionManager manager=getSystemService(MediaProjectionManager.class);
        // Horizon supplies the compositor view through its own consent flow.
        // Do not force Android's default physical display, which can be blank in VR.
        startActivityForResult(manager.createScreenCaptureIntent(),71);}}
    @Override protected void onActivityResult(int request,int result,Intent data){super.onActivityResult(request,result,data);if(request==71){if(result==RESULT_OK&&data!=null)startForegroundService(new Intent(this,SharedViewService.class).putExtra("result",result).putExtra("projection",data));finish();}}
}

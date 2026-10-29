// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.app.*;
import android.os.*;
import org.json.*;
public final class QuickAccessProbe extends Instrumentation {
    @Override public void onCreate(Bundle args){super.onCreate(args);start();}
    @Override public void onStart(){Bundle result=new Bundle();JSONObject state=QuickAccess.status(getTargetContext());
        result.putString("launcher_status",state.toString());
        result.putBoolean("configuration_intent_resolves",QuickAccess.configurationIntent(getTargetContext()).resolveActivity(getTargetContext().getPackageManager())!=null);
        finish(state.has("error")?Activity.RESULT_CANCELED:Activity.RESULT_OK,result);
    }
}

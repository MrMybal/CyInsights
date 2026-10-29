// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.content.*;
/** Non-exported callback sent only through our own PendingIntent. */
public final class ShortcutPinnedReceiver extends BroadcastReceiver {
    @Override public void onReceive(Context context,Intent intent){context.getSharedPreferences("quick_access",0).edit().putBoolean("pin_confirmed",true).apply();android.util.Log.i("CyInsightOVR","Launcher confirmed configuration shortcut creation");}
}

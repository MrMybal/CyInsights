// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.app.*;
import android.content.*;
import android.content.pm.*;
import android.graphics.drawable.Icon;
import org.json.*;

/** Public launcher shortcut API; an accepted request is not treated as a pinned icon. */
final class QuickAccess {
    static final String ID="cyinsightovr.configure";
    static Intent configurationIntent(Context context){return new Intent(context,MainActivity.class).setAction(Intent.ACTION_MAIN)
        .addCategory(Intent.CATEGORY_LAUNCHER).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK|Intent.FLAG_ACTIVITY_CLEAR_TOP).putExtra("open_settings",true);}
    static JSONObject status(Context context){JSONObject out=new JSONObject();
        try{ShortcutManager manager=context.getSystemService(ShortcutManager.class);boolean pinned=false;
            if(manager!=null)for(ShortcutInfo info:manager.getPinnedShortcuts())if(ID.equals(info.getId())&&info.isEnabled())pinned=true;
            out.put("pin_supported",manager!=null&&manager.isRequestPinShortcutSupported()).put("configuration_pinned",pinned)
                .put("callback_received",context.getSharedPreferences("quick_access",0).getBoolean("pin_confirmed",false));
        }catch(Exception error){try{out.put("error",error.getMessage());}catch(JSONException ignored){}}
        return out;
    }
    static void pin(Activity activity){
        try{
            ShortcutManager manager=activity.getSystemService(ShortcutManager.class);
            if(manager==null||!manager.isRequestPinShortcutSupported()){show(activity);return;}
            ShortcutInfo shortcut=new ShortcutInfo.Builder(activity,ID).setActivity(new ComponentName(activity,MainActivity.class))
                .setShortLabel("CyInsightOVR").setLongLabel("Configure CyInsightOVR")
                .setIcon(Icon.createWithResource(activity,activity.getApplicationInfo().icon)).setIntent(configurationIntent(activity)).build();
            PendingIntent callback=PendingIntent.getBroadcast(activity,81,new Intent(activity,ShortcutPinnedReceiver.class),PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE);
            boolean accepted=manager.requestPinShortcut(shortcut,callback.getIntentSender());
            android.util.Log.i("CyInsightOVR","Configuration pin request accepted="+accepted);
            if(!accepted)new AlertDialog.Builder(activity).setTitle("Shortcut not added").setMessage("The launcher did not accept the shortcut request. Open Library > Unknown Sources > CyInsightOVR.").setPositiveButton("OK",null).show();
        }catch(Exception error){new AlertDialog.Builder(activity).setTitle("Shortcut unavailable").setMessage(error.getMessage()).setPositiveButton("OK",null).show();}
    }
    static void show(Activity activity){
        JSONObject state=status(activity);String message;
        if(state.optBoolean("configuration_pinned"))message="Your configuration shortcut is pinned in the launcher's shortcut list. Select CyInsightOVR there to open Display & HUD directly.";
        else if(state.optBoolean("pin_supported"))message="Add a CyInsightOVR icon that opens Display & HUD directly. Confirm the addition in the Meta launcher window. The launcher chooses where this shortcut appears.";
        else message="This launcher does not offer Android shortcut pinning. Open Library > Unknown Sources > CyInsightOVR. The connected-PC launcher can also open configuration directly.";
        AlertDialog.Builder dialog=new AlertDialog.Builder(activity).setTitle("Quick configuration access").setMessage(message).setNegativeButton("Close",null);
        if(state.optBoolean("pin_supported")&&!state.optBoolean("configuration_pinned"))dialog.setPositiveButton("Add configuration shortcut",(d,w)->pin(activity));dialog.show();
    }
}

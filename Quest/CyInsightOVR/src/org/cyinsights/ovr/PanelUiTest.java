// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.app.*;
import android.content.*;
import android.os.*;
import android.view.*;
import org.json.*;
/** Exercises actual Android input -> native scrolling and saved UI choices; opens no game. */
public final class PanelUiTest extends Instrumentation {
    private MainActivity main;
    private void touch(int action,float x,float y){runOnMainSync(()->{long t=SystemClock.uptimeMillis();MotionEvent e=MotionEvent.obtain(t,t,action,x,y,0);main.renderViewForTest().dispatchTouchEvent(e);e.recycle();});}
    private void check(boolean condition,String message)throws Exception{if(!condition)throw new Exception(message);}
    @Override public void onCreate(Bundle args){super.onCreate(args);start();}
    @Override public void onStart(){Bundle out=new Bundle();String original=null;Context context=getTargetContext();
        try{
            original=context.getSharedPreferences("MainActivity",0).getString("display_settings",null);
            Intent intent=new Intent(context,MainActivity.class).setAction(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            main=(MainActivity)startActivitySync(intent);
            try(android.os.ParcelFileDescriptor bring=getUiAutomation().executeShellCommand("am start -a android.intent.action.MAIN -c android.intent.category.LAUNCHER -n org.cyinsights.ovr/.MainActivity")){}
            long readyDeadline=SystemClock.elapsedRealtime()+15000;
            while(SystemClock.elapsedRealtime()<readyDeadline){JSONObject frame=main.uiReportForTest();if(frame.optBoolean("activity_active")&&frame.optInt("rendered_frames")>4)break;Thread.sleep(200);}
            main.pageForTest(2);Thread.sleep(700);
            JSONObject before=main.uiReportForTest();check(before.getInt("page")==2&&before.getDouble("scroll_max")>50,"Settings page did not have real scrollable content: "+before);
            float x=(float)before.getDouble("choice_x"),y=(float)before.getDouble("choice_y");int mask=before.getInt("chart_mask");
            // Drag starting over a checkbox must scroll without toggling it.
            touch(MotionEvent.ACTION_DOWN,x,y);touch(MotionEvent.ACTION_MOVE,x,y-110);touch(MotionEvent.ACTION_UP,x,y-110);Thread.sleep(350);
            JSONObject after=main.uiReportForTest();check(after.getDouble("scroll")>50&&after.getInt("chart_mask")==mask,"Drag did not scroll, or activated a checkbox");out.putBoolean("drag_scroll_without_click",true);
            runOnMainSync(()->main.renderViewForTest().dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN,KeyEvent.KEYCODE_PAGE_UP)));Thread.sleep(250);
            check(main.uiReportForTest().getDouble("scroll")<5,"Controller/page-up scroll did not return to top");out.putBoolean("key_scroll",true);
            // Standard Android AXIS_VSCROLL is how Quest panels can forward stick scrolling.
            final float wheelX=x,wheelY=y;
            runOnMainSync(()->{
                MotionEvent.PointerProperties p=new MotionEvent.PointerProperties();p.id=0;p.toolType=MotionEvent.TOOL_TYPE_MOUSE;
                MotionEvent.PointerCoords c=new MotionEvent.PointerCoords();c.x=wheelX;c.y=wheelY;c.setAxisValue(MotionEvent.AXIS_VSCROLL,-3);
                long t=SystemClock.uptimeMillis();MotionEvent e=MotionEvent.obtain(t,t,MotionEvent.ACTION_SCROLL,1,new MotionEvent.PointerProperties[]{p},new MotionEvent.PointerCoords[]{c},0,0,1,1,0,0,InputDevice.SOURCE_MOUSE,0);
                main.renderViewForTest().dispatchGenericMotionEvent(e);e.recycle();
            });Thread.sleep(450);check(main.uiReportForTest().getDouble("scroll")>20,"Generic motion wheel did not scroll native content");out.putBoolean("generic_motion_scroll",true);
            runOnMainSync(()->main.renderViewForTest().dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN,KeyEvent.KEYCODE_PAGE_UP)));Thread.sleep(250);
            before=main.uiReportForTest();x=(float)before.getDouble("choice_x");y=(float)before.getDouble("choice_y");
            touch(MotionEvent.ACTION_DOWN,x,y);Thread.sleep(120);touch(MotionEvent.ACTION_UP,x,y);Thread.sleep(500);
            int expected=mask^1;check(main.uiReportForTest().getInt("chart_mask")==expected,"Tap did not toggle the rendered chart choice: "+main.uiReportForTest());
            JSONObject saved=new JSONObject(context.getSharedPreferences("MainActivity",0).getString("display_settings","{}"));check(saved.getInt("chart_mask")==expected,"Native display choice was not persisted");out.putBoolean("rendered_choice_saved",true);
            final MainActivity old=main;runOnMainSync(()->old.finish());Thread.sleep(650);main=(MainActivity)startActivitySync(intent);Thread.sleep(1300);
            check(main.uiReportForTest().getInt("chart_mask")==expected,"Choice lost when panel reopened");out.putBoolean("choice_reopened",true);
            out.putString("result","Real native page scrolled by drag, key and generic motion; choice saved and reopened");
        }catch(Exception e){out.putString("error",e.toString());}
        finally{android.content.SharedPreferences.Editor edit=context.getSharedPreferences("MainActivity",0).edit();if(original==null)edit.remove("display_settings");else edit.putString("display_settings",original);edit.commit();}
        finish(out.containsKey("error")?Activity.RESULT_CANCELED:Activity.RESULT_OK,out);
    }
}

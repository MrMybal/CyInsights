// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;

import android.os.*;
import android.view.*;

/** Routes Quest panel pointer/scroll input to the GL thread without turning drags into clicks. */
final class PanelInput implements View.OnTouchListener, View.OnGenericMotionListener, View.OnKeyListener {
    interface Sink {
        void pointer(float x,float y,boolean down);
        void hover(float x,float y);
        void wheel(float x,float y,float horizontal,float vertical);
        void scroll(float pixels);
    }
    private final Sink sink;private final float slop,density;private final Handler ui=new Handler(Looper.getMainLooper());
    private float startX,startY,lastY,stick;private boolean pressed,dragging;
    PanelInput(View view,Sink sink){this.sink=sink;slop=ViewConfiguration.get(view.getContext()).getScaledTouchSlop();density=view.getResources().getDisplayMetrics().density;}
    private final Runnable repeat=new Runnable(){public void run(){if(Math.abs(stick)>.2f){sink.scroll(stick*12*density);ui.postDelayed(this,16);}}};
    void cancel(){if(pressed)sink.pointer(-10000,-10000,false);pressed=false;dragging=false;stick=0;ui.removeCallbacks(repeat);}
    public boolean onTouch(View view,MotionEvent e){
        if(e.isFromSource(InputDevice.SOURCE_MOUSE)){
            sink.pointer(e.getX(),e.getY(),e.getActionMasked()!=MotionEvent.ACTION_UP&&e.getActionMasked()!=MotionEvent.ACTION_CANCEL);return true;
        }
        switch(e.getActionMasked()){
            case MotionEvent.ACTION_DOWN:
                view.requestFocus();startX=e.getX();startY=lastY=e.getY();pressed=true;dragging=false;sink.pointer(startX,startY,true);return true;
            case MotionEvent.ACTION_MOVE:
                if(!pressed)return false;
                if(!dragging&&Math.hypot(e.getX()-startX,e.getY()-startY)>slop){dragging=true;lastY=startY;sink.pointer(-10000,-10000,false);}
                if(dragging){sink.scroll(lastY-e.getY());lastY=e.getY();}return true;
            case MotionEvent.ACTION_UP:
                if(pressed&&!dragging){sink.pointer(e.getX(),e.getY(),false);view.performClick();}
                pressed=false;dragging=false;return true;
            case MotionEvent.ACTION_CANCEL:if(pressed)sink.pointer(-10000,-10000,false);pressed=false;dragging=false;return true;
            default:return false;
        }
    }
    public boolean onGenericMotion(View view,MotionEvent e){
        if(e.getActionMasked()==MotionEvent.ACTION_SCROLL){
            sink.wheel(e.getX(),e.getY(),e.getAxisValue(MotionEvent.AXIS_HSCROLL),e.getAxisValue(MotionEvent.AXIS_VSCROLL));return true;
        }
        if(e.getActionMasked()==MotionEvent.ACTION_HOVER_MOVE||e.getActionMasked()==MotionEvent.ACTION_HOVER_ENTER){sink.hover(e.getX(),e.getY());return true;}
        if(e.getActionMasked()==MotionEvent.ACTION_MOVE&&e.isFromSource(InputDevice.SOURCE_JOYSTICK)){
            stick=e.getAxisValue(MotionEvent.AXIS_Y);if(Math.abs(stick)<.2f)stick=e.getAxisValue(MotionEvent.AXIS_RY);
            ui.removeCallbacks(repeat);if(Math.abs(stick)>.2f)ui.post(repeat);return true;
        }
        return false;
    }
    public boolean onKey(View view,int key,KeyEvent e){
        float step;
        switch(key){case KeyEvent.KEYCODE_DPAD_DOWN:step=80*density;break;case KeyEvent.KEYCODE_DPAD_UP:step=-80*density;break;
            case KeyEvent.KEYCODE_PAGE_DOWN:step=view.getHeight()*.7f;break;case KeyEvent.KEYCODE_PAGE_UP:step=-view.getHeight()*.7f;break;default:return false;}
        if(e.getAction()==KeyEvent.ACTION_DOWN)sink.scroll(step);return true;
    }
}

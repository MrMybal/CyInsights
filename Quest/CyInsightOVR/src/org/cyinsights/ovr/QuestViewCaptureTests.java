// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import java.io.*;import java.nio.*;
/** Explicit raw-layout fixtures; never starts a display capture. */
public final class QuestViewCaptureTests {
    static byte[] fixture(int width,int height,int format){ByteBuffer b=ByteBuffer.allocate(16+width*height*4).order(ByteOrder.LITTLE_ENDIAN);b.putInt(width).putInt(height).putInt(format).putInt(1);for(int i=0;i<width*height;++i)b.put((byte)11).put((byte)22).put((byte)33).put((byte)0);return b.array();}
    static void check(boolean ok,String why){if(!ok)throw new AssertionError(why);}
    public static void main(String[] args)throws Exception{
        QuestViewCapture.Pixels image=QuestViewCapture.read(new ByteArrayInputStream(fixture(2,2,1)));check(image.width==2&&image.height==2&&image.argb[0]==0xff0b1621,"RGBA source channels or unused alpha changed");
        image=QuestViewCapture.read(new ByteArrayInputStream(fixture(2560,1440,2)));check(image.width==1280&&image.height==720&&image.argb.length==921600,"Bounded aspect-preserving sampling failed");
        try{QuestViewCapture.read(new ByteArrayInputStream(fixture(2,2,4)));throw new AssertionError("Unknown layout accepted");}catch(IOException expected){}
        byte[] truncated=fixture(2,2,1);try{QuestViewCapture.read(new ByteArrayInputStream(java.util.Arrays.copyOf(truncated,truncated.length-1)));throw new AssertionError("Truncated pixels accepted");}catch(EOFException expected){}
        byte[] extra=java.util.Arrays.copyOf(truncated,truncated.length+1);try{QuestViewCapture.read(new ByteArrayInputStream(extra));throw new AssertionError("Trailing pixels accepted");}catch(IOException expected){}
        ByteBuffer.wrap(truncated).order(ByteOrder.LITTLE_ENDIAN).putInt(9000);try{QuestViewCapture.read(new ByteArrayInputStream(truncated));throw new AssertionError("Unbounded dimensions accepted");}catch(IOException expected){}
        check(QuestViewCapture.targetResumed("  topResumedActivity=ActivityRecord{123 u0 org.fixture.game/.Main t2}","org.fixture.game"),"Resumed target not recognized");
        check(!QuestViewCapture.targetResumed("  mLastPausedActivity: ActivityRecord{123 u0 org.fixture.game/.Main t2}\n topResumedActivity=ActivityRecord{124 u0 org.fixture.game.other/.Main t3}","org.fixture.game"),"Another app or an old activity was accepted as the capture target");
        System.out.println("8 bounded Meta raw image layout and foreground checks passed (explicit fixtures)");
    }
}

// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.content.*;
import android.database.*;
import android.net.Uri;
import android.os.*;
import java.io.*;

/** A scoped URI grant makes the layer visible and authenticates its Binder data channel. */
public final class LayerVisibilityProvider extends ContentProvider {
    static final Uri URI=Uri.parse("content://org.cyinsights.ovr.layer/visibility");
    @Override public boolean onCreate(){return true;}
    @Override public String getType(Uri uri){return URI.equals(uri)?"vnd.android.cursor.item/vnd.cyinsights.layer":null;}
    @Override public Cursor query(Uri uri,String[] projection,String selection,String[] args,String sort){if(!URI.equals(uri))throw new IllegalArgumentException("Unknown layer URI");MatrixCursor cursor=new MatrixCursor(new String[]{"protocol"});cursor.addRow(new Object[]{1});return cursor;}
    @Override public ParcelFileDescriptor openFile(Uri uri,String mode)throws FileNotFoundException{
        if(!URI.equals(uri)||!"rw".equals(mode))throw new FileNotFoundException("Unknown layer channel");
        try{return GpuDataService.openLayerChannel(Binder.getCallingUid(),Binder.getCallingPid());}
        catch(IOException error){throw new FileNotFoundException(error.getMessage());}
    }
    @Override public Uri insert(Uri uri,ContentValues values){throw new UnsupportedOperationException();}
    @Override public int update(Uri uri,ContentValues values,String where,String[] args){throw new UnsupportedOperationException();}
    @Override public int delete(Uri uri,String where,String[] args){throw new UnsupportedOperationException();}
}

// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.content.*;
import android.database.*;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;
import java.io.*;
public final class UpdateFileProvider extends ContentProvider {
    static final Uri URI=Uri.parse("content://org.cyinsights.ovr.updates/current.apk");
    private File file(Uri uri)throws FileNotFoundException{if(!URI.equals(uri))throw new FileNotFoundException("Unknown update URI");return new File(getContext().getFilesDir(),"Updates/update.apk");}
    @Override public boolean onCreate(){return true;}
    @Override public String getType(Uri uri){return URI.equals(uri)?"application/vnd.android.package-archive":null;}
    @Override public ParcelFileDescriptor openFile(Uri uri,String mode)throws FileNotFoundException{if(!mode.equals("r"))throw new FileNotFoundException("Update access is read-only");return ParcelFileDescriptor.open(file(uri),ParcelFileDescriptor.MODE_READ_ONLY);}
    @Override public Cursor query(Uri uri,String[] projection,String selection,String[] args,String sort){
        try{File f=file(uri);String[] columns=projection==null?new String[]{OpenableColumns.DISPLAY_NAME,OpenableColumns.SIZE}:projection;MatrixCursor c=new MatrixCursor(columns);Object[] row=new Object[columns.length];for(int i=0;i<columns.length;++i)row[i]=OpenableColumns.DISPLAY_NAME.equals(columns[i])?"CyInsightOVR-update.apk":OpenableColumns.SIZE.equals(columns[i])?f.length():null;c.addRow(row);return c;}catch(FileNotFoundException e){throw new IllegalArgumentException("Unknown update URI");}
    }
    @Override public Uri insert(Uri uri,ContentValues values){throw new UnsupportedOperationException();}
    @Override public int update(Uri uri,ContentValues values,String where,String[] args){throw new UnsupportedOperationException();}
    @Override public int delete(Uri uri,String where,String[] args){throw new UnsupportedOperationException();}
}

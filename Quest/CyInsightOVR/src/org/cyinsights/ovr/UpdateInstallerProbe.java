// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.app.*;
import android.content.*;
import android.os.Bundle;
import java.io.*;
public final class UpdateInstallerProbe extends Instrumentation {
    @Override public void onCreate(Bundle b){super.onCreate(b);start();}
    private static void check(boolean good,String why){if(!good)throw new AssertionError(why);}
    @Override public void onStart(){
        Bundle out=new Bundle();int checks=0;
        try{
            Context c=getTargetContext();
            String temp=System.getProperty("java.io.tmpdir");
            try{System.setProperty("java.io.tmpdir",c.getCacheDir().getAbsolutePath());out.putInt("policy_checks",ReleaseUpdateTests.run(new String[]{"--network"}));}
            finally{if(temp!=null)System.setProperty("java.io.tmpdir",temp);}
            File root=new File(c.getFilesDir(),"UpdateFixtures");File fixture=new File(root,"same-signer.apk"),wrong=new File(root,"wrong-signer.apk");
            ReleaseUpdate.Candidate good=new ReleaseUpdate.Candidate("v9.0.0","fixture.apk","https://github.com/",ReleaseUpdate.hash(fixture),fixture.length());
            GithubUpdater.verifyApk(c,fixture,good,"vulkan");++checks;
            boolean rejected=false;try{GithubUpdater.verifyApk(c,fixture,good,"standard");}catch(Exception expected){rejected=true;}check(rejected,"Wrong variant was accepted");++checks;
            ReleaseUpdate.Candidate bad=new ReleaseUpdate.Candidate("v9.0.0","fixture.apk","https://github.com/",ReleaseUpdate.hash(wrong),wrong.length());
            rejected=false;try{GithubUpdater.verifyApk(c,wrong,bad,"vulkan");}catch(Exception expected){rejected=true;}check(rejected,"Wrong signer was accepted");++checks;
            File own=new File(c.getApplicationInfo().sourceDir);bad=new ReleaseUpdate.Candidate("v0.2.2-alpha.1","fixture.apk","https://github.com/",ReleaseUpdate.hash(own),own.length());
            rejected=false;try{GithubUpdater.verifyApk(c,own,bad,"vulkan");}catch(Exception expected){rejected=true;}check(rejected,"Alpha APK was accepted");++checks;
            File dir=new File(c.getFilesDir(),"Updates");dir.mkdirs();File served=new File(dir,"update.apk");check(!served.exists(),"A real update download exists; refusing to overwrite it");
            try{
                try(InputStream in=new FileInputStream(fixture);OutputStream dest=new FileOutputStream(served)){byte[] data=new byte[65536];int n;while((n=in.read(data))!=-1)dest.write(data,0,n);}
                long count=0;try(InputStream in=c.getContentResolver().openInputStream(UpdateFileProvider.URI)){byte[] data=new byte[65536];int n;while((n=in.read(data))!=-1)count+=n;}check(count==fixture.length(),"APK provider returned the wrong content");++checks;
                rejected=false;try{c.getContentResolver().openInputStream(android.net.Uri.parse("content://org.cyinsights.ovr.updates/../private"));}catch(Exception expected){rejected=true;}check(rejected,"Provider accepted an unexpected path");++checks;
                rejected=false;try{c.getContentResolver().openFileDescriptor(UpdateFileProvider.URI,"w");}catch(Exception expected){rejected=true;}check(rejected,"Provider allowed writes");++checks;
            }finally{served.delete();}
            Intent installer=new Intent(Intent.ACTION_INSTALL_PACKAGE).setDataAndType(UpdateFileProvider.URI,"application/vnd.android.package-archive").addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            android.content.pm.ResolveInfo handler=c.getPackageManager().resolveActivity(installer,0);
            out.putBoolean("installer_activity_available",handler!=null);out.putBoolean("install_permission_granted",c.getPackageManager().canRequestPackageInstalls());out.putInt("checks",checks);out.putString("result","passed");finish(Activity.RESULT_OK,out);
        }catch(Throwable e){out.putString("error",e.toString());out.putInt("checks",checks);finish(Activity.RESULT_CANCELED,out);}
    }
}

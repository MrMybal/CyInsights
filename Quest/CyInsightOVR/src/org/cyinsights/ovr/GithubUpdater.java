// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.app.Activity;
import android.content.*;
import android.content.pm.*;
import android.net.Uri;
import android.provider.Settings;
import org.json.*;
import javax.net.ssl.HttpsURLConnection;
import java.net.URL;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.zip.ZipFile;

final class GithubUpdater implements AutoCloseable {
    interface Listener {void changed(JSONObject state);}
    private final Activity activity;private final Context context;private final Listener listener;
    private final ExecutorService worker=Executors.newSingleThreadExecutor();
    private final AtomicBoolean cancel=new AtomicBoolean();private final SharedPreferences preferences;
    private volatile boolean busy=false,closed=false,automatic;private volatile long received=0,total=0;
    private volatile String phase="idle",message="Check GitHub for a newer stable release.";
    private volatile ReleaseUpdate.Candidate candidate;
    private final File directory,apk,metadata;
    GithubUpdater(Activity a,Listener l){
        activity=a;context=a.getApplicationContext();listener=l;preferences=context.getSharedPreferences("github_updates",0);automatic=preferences.getBoolean("automatic",true);
        directory=new File(context.getFilesDir(),"Updates");apk=new File(directory,"update.apk");metadata=new File(directory,"release.json");
        emit();long elapsed=System.currentTimeMillis()-preferences.getLong("last_check",0);
        if(automatic&&(elapsed<0||elapsed>=86400000L||preferences.getLong("last_check",0)==0))check();
        else worker.execute(()->{try{restore();}catch(Exception ignored){}emit();});
    }
    private void emit(){if(closed)return;try{JSONObject j=new JSONObject().put("installed",UpdateBuild.VERSION).put("variant",UpdateBuild.VARIANT).put("automatic",automatic).put("busy",busy).put("phase",phase).put("message",message).put("downloaded",received).put("total",total);ReleaseUpdate.Candidate c=candidate;if(c!=null)j.put("available",c.version);listener.changed(j);}catch(JSONException ignored){}}
    void action(String action){
        if(action.equals("update_auto")){automatic=!automatic;preferences.edit().putBoolean("automatic",automatic).apply();emit();return;}
        if(action.equals("update_cancel")){cancel.set(true);message="Cancelling...";emit();return;}
        if(action.equals("update_check"))check();else if(action.equals("update_download"))download();else if(action.equals("update_install"))install();
    }
    private void begin(String text,Runnable action){if(busy||closed)return;busy=true;cancel.set(false);message=text;emit();worker.execute(()->{try{action.run();}finally{busy=false;emit();}});}
    private void failed(Exception e){phase="error";message=cancel.get()?"Update cancelled.":"Update failed: "+(e.getMessage()==null?e.getClass().getSimpleName():e.getMessage());}
    private void check(){
        begin("Checking GitHub stable releases...",()->{
            try{preferences.edit().putLong("last_check",System.currentTimeMillis()).apply();phase="checking";received=total=0;candidate=null;emit();
                ByteArrayOutputStream data=new ByteArrayOutputStream();read(ReleaseUpdate.API,2*1024*1024,data,false);
                candidate=ReleaseUpdate.select(new JSONObject(new String(data.toByteArray(),StandardCharsets.UTF_8)),UpdateBuild.VERSION,UpdateBuild.VARIANT);
                if(candidate==null){phase="current";message="No newer compatible stable release is available. Alpha and other prereleases are ignored.";}
                else {phase="available";message="A newer stable release is available.";if(apk.isFile())try{verifyApk(context,apk,candidate,UpdateBuild.VARIANT);phase="ready";message="APK verified. Ready to install.";}catch(Exception ignored){}}
            }catch(Exception e){failed(e);}
        });
    }
    private void restore()throws Exception{
        if(!metadata.isFile()||metadata.length()>16384)return;JSONObject saved=new JSONObject(readSmall(metadata));
        JSONObject a=new JSONObject().put("name",saved.getString("name")).put("browser_download_url",saved.getString("url")).put("digest","sha256:"+saved.getString("sha256")).put("size",saved.getLong("size"));
        JSONObject release=new JSONObject().put("draft",false).put("prerelease",false).put("tag_name",saved.getString("tag")).put("html_url","https://github.com/MrMybal/CyInsights/releases/tag/"+saved.getString("tag")).put("assets",new JSONArray().put(a));
        ReleaseUpdate.Candidate c=ReleaseUpdate.select(release,UpdateBuild.VERSION,UpdateBuild.VARIANT);if(c==null)return;
        verifyApk(context,apk,c,UpdateBuild.VARIANT);candidate=c;phase="ready";message="Previously downloaded APK verified. Ready to install.";
    }
    private void download(){
        ReleaseUpdate.Candidate c=candidate;if(c==null)return;
        begin("Downloading and verifying the APK...",()->{
            File partial=new File(directory,"update.partial.apk");
            try{phase="downloading";received=0;total=c.size;if(!directory.isDirectory()&&!directory.mkdirs())throw new IOException("Cannot create private update storage");
                try(OutputStream out=new FileOutputStream(partial)){read(c.url,c.size,out,true);}
                if(cancel.get())throw new IOException("Cancelled");verifyApk(context,partial,c,UpdateBuild.VARIANT);
                if(apk.exists()&&!apk.delete())throw new IOException("Cannot replace the previous APK download");
                if(!partial.renameTo(apk))throw new IOException("Cannot finish saving the APK");
                try(OutputStream out=new FileOutputStream(metadata)){out.write(c.json().toString().getBytes(StandardCharsets.UTF_8));}
                phase="ready";message="APK size, SHA-256, version, variant and signer verified. Ready to install.";
            }catch(Exception e){failed(e);}finally{if(partial.exists())partial.delete();}
        });
    }
    private void read(String initial,long limit,OutputStream out,boolean progress)throws Exception{
        String next=initial;
        for(int redirect=0;redirect<6;++redirect){
            if(cancel.get())throw new IOException("Cancelled");if(!ReleaseUpdate.allowedHttps(next))throw new IOException("Unexpected download host or insecure URL");
            HttpsURLConnection connection=(HttpsURLConnection)new URL(next).openConnection();connection.setInstanceFollowRedirects(false);connection.setConnectTimeout(5000);connection.setReadTimeout(10000);
            connection.setRequestProperty("User-Agent","CyInsightOVR-Updater/1");connection.setRequestProperty("Accept","application/vnd.github+json");connection.setRequestProperty("X-GitHub-Api-Version","2022-11-28");
            try{int status=connection.getResponseCode();if(status==301||status==302||status==303||status==307||status==308){String location=connection.getHeaderField("Location");if(location==null)throw new IOException("Missing HTTPS redirect");next=new URL(new URL(next),location).toExternalForm();continue;}
                if(status!=200)throw new IOException("GitHub HTTP "+status+(status==403||status==429?": rate limit reached; try again later":status==404?": no public stable release is available":""));
                long count=0,lastEmit=0;try(InputStream in=connection.getInputStream()){byte[] data=new byte[65536];int n;while((n=in.read(data))!=-1){if(cancel.get())throw new IOException("Cancelled");count+=n;if(count>limit)throw new IOException("Update response exceeds the declared size");out.write(data,0,n);if(progress){received=count;long now=System.nanoTime();if(now-lastEmit>100000000L){emit();lastEmit=now;}}}}
                return;
            }finally{connection.disconnect();}
        }throw new IOException("Too many GitHub redirects");
    }
    static void verifyApk(Context context,File apk,ReleaseUpdate.Candidate c,String variant)throws Exception{
        ReleaseUpdate.verify(apk,c);ReleaseUpdate.Version v=ReleaseUpdate.version(c.tag);if(v==null||v.pre)throw new IOException("Only stable releases can be installed");
        PackageManager pm=context.getPackageManager();PackageInfo current=pm.getPackageInfo(context.getPackageName(),PackageManager.GET_SIGNING_CERTIFICATES);
        PackageInfo next=pm.getPackageArchiveInfo(apk.getAbsolutePath(),PackageManager.GET_SIGNING_CERTIFICATES);
        if(next==null)throw new IOException("Android could not read the downloaded APK package metadata");
        if(!context.getPackageName().equals(next.packageName))throw new IOException("APK package identity does not match CyInsightOVR");
        if(!c.version.equals(next.versionName))throw new IOException("APK version does not match the selected GitHub release");
        if(next.getLongVersionCode()<=current.getLongVersionCode())throw new IOException("APK version code must be newer than the installed application");
        if(!signers(current).equals(signers(next)))throw new IOException("APK signer differs from the installed application");
        try(ZipFile zip=new ZipFile(apk)){
            boolean layer=zip.getEntry("lib/arm64-v8a/libVkLayer_CyInsightOVR.so")!=null;
            if(layer!=variant.equals("vulkan")||zip.getEntry("lib/arm64-v8a/libcyinsightovr.so")==null)throw new IOException("APK variant or architecture mismatch");
            java.util.zip.ZipEntry info=zip.getEntry("assets/update-build.json");if(info==null||info.getSize()>4096)throw new IOException("APK build metadata is missing");
            try(InputStream in=zip.getInputStream(info)){ByteArrayOutputStream data=new ByteArrayOutputStream();byte[] b=new byte[1024];int n;while((n=in.read(b))!=-1){if(data.size()+n>4096)throw new IOException("Invalid APK build metadata");data.write(b,0,n);}JSONObject build=new JSONObject(new String(data.toByteArray(),StandardCharsets.UTF_8));if(!variant.equals(build.optString("variant"))||!c.version.equals(build.optString("version")))throw new IOException("APK build metadata mismatch");}
        }
    }
    private static Set<String> signers(PackageInfo p)throws Exception{
        if(p.signingInfo==null)throw new IOException("APK signing information is unavailable");Set<String> hashes=new HashSet<>();
        for(Signature s:p.signingInfo.getApkContentsSigners())hashes.add(ReleaseUpdate.hex(MessageDigest.getInstance("SHA-256").digest(s.toByteArray())));
        if(hashes.isEmpty())throw new IOException("APK signing certificate is unavailable");return hashes;
    }
    private void install(){
        ReleaseUpdate.Candidate c=candidate;if(c==null||!phase.equals("ready"))return;
        begin("Verifying the APK before installation...",()->{
            try{verifyApk(context,apk,c,UpdateBuild.VARIANT);
                try{JSONObject state=BridgeClient.request(context,new JSONObject().put("operation","snapshot"));JSONObject recording=state.optJSONObject("tile_recording");if(state.optBoolean("capturing")||recording!=null&&recording.optBoolean("active"))throw new IllegalStateException("Stop the active GPU recording before installing");}catch(IllegalStateException active){throw active;}catch(Exception offline){/* An offline collector does not prevent updating the app. */}
                activity.runOnUiThread(()->{
                    if(closed||activity.isFinishing())return;
                    try{
                        if(!context.getPackageManager().canRequestPackageInstalls()){
                            message="Allow this app to install updates in Android settings, then press Install again.";phase="ready";
                            activity.startActivity(new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,Uri.parse("package:"+context.getPackageName())));
                        }else{
                            Intent intent=new Intent(Intent.ACTION_INSTALL_PACKAGE).setDataAndType(UpdateFileProvider.URI,"application/vnd.android.package-archive").addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION).putExtra(Intent.EXTRA_RETURN_RESULT,true);
                            message="Waiting for Android installation confirmation.";phase="ready";activity.startActivityForResult(intent,941);
                        }
                    }catch(ActivityNotFoundException unavailable){phase="ready";message="This Quest firmware does not expose the Android installer/settings window. Install the verified APK through authorized ADB instead.";}
                    catch(Exception error){failed(error);}emit();
                });
            }catch(Exception e){failed(e);}
        });
    }
    void installationResult(int result){phase="ready";message=result==Activity.RESULT_OK?"Android accepted the update. Reopen CyInsightOVR if needed.":"Installation was cancelled or did not complete. The downloaded APK is kept for retry.";emit();}
    private static String readSmall(File f)throws IOException{try(InputStream in=new FileInputStream(f)){ByteArrayOutputStream out=new ByteArrayOutputStream();byte[] data=new byte[1024];int n;while((n=in.read(data))!=-1){if(out.size()+n>16384)throw new IOException("Invalid saved update metadata");out.write(data,0,n);}return new String(out.toByteArray(),StandardCharsets.UTF_8);}}
    @Override public void close(){closed=true;cancel.set(true);worker.shutdownNow();}
}

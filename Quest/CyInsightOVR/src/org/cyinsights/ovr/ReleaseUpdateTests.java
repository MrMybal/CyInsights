// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import org.json.*;
import java.io.*;
import java.net.URL;
import javax.net.ssl.HttpsURLConnection;
public final class ReleaseUpdateTests {
    private static int checks;
    private static void check(boolean good,String why){++checks;if(!good)throw new AssertionError(why);}
    private interface Attempt{void run()throws Exception;}
    private static void reject(Attempt attempt,String why)throws Exception{boolean failed=false;try{attempt.run();}catch(Exception expected){failed=true;}check(failed,why);}
    private static JSONObject release(String tag,String variant)throws Exception{
        String name="CyInsights-"+tag+"-Quest-"+(variant.equals("vulkan")?"Vulkan":"Standard")+"-arm64.apk";
        JSONObject asset=new JSONObject().put("name",name).put("browser_download_url","https://github.com/MrMybal/CyInsights/releases/download/"+tag+"/"+name).put("digest","sha256:"+String.join("",java.util.Collections.nCopies(64,"a"))).put("size",3);
        return new JSONObject().put("draft",false).put("prerelease",false).put("tag_name",tag).put("html_url","https://github.com/MrMybal/CyInsights/releases/tag/"+tag).put("assets",new JSONArray().put(asset));
    }
    static int run(String[] args)throws Exception{
        checks=0;
        check(ReleaseUpdate.select(release("v9.0.0","vulkan"),"0.2.2-alpha.1","vulkan")!=null,"Future stable accepted");
        for(String tag:new String[]{"v0.2.1","v0.2.2-alpha.2","v9.0.0-beta.1","v9.0.0-rc.1","v99.0.0+build","v09.0.0","not-a-version"})check(ReleaseUpdate.select(release(tag,"vulkan"),"0.2.2-alpha.1","vulkan")==null,"Old/prerelease/invalid excluded");
        check(ReleaseUpdate.select(release("v0.2.2","vulkan"),"0.2.2-alpha.1","vulkan")!=null,"Stable replaces installed alpha");check(ReleaseUpdate.select(release("v0.2.2","vulkan"),"0.2.2","vulkan")==null,"Equal version excluded");
        for(String flag:new String[]{"draft","prerelease"})check(ReleaseUpdate.select(release("v9.0.0","vulkan").put(flag,true),"0.2.2","vulkan")==null,"Release flag excludes candidate");
        check(ReleaseUpdate.select(release("v9.0.0","standard"),"0.2.2","vulkan")==null,"Variant preserved");
        check(ReleaseUpdate.select(release("v9.0.0","standard"),"0.2.2","standard")!=null,"Standard selected");
        for(String field:new String[]{"digest","browser_download_url"}){JSONObject j=release("v9.0.0","vulkan");j.getJSONArray("assets").getJSONObject(0).put(field,"invalid");reject(()->ReleaseUpdate.select(j,"0.2.2","vulkan"),"Bad asset provenance rejected");}
        JSONObject duplicate=release("v9.0.0","vulkan");duplicate.getJSONArray("assets").put(duplicate.getJSONArray("assets").getJSONObject(0));reject(()->ReleaseUpdate.select(duplicate,"0.2.2","vulkan"),"Duplicate asset rejected");
        for(String url:new String[]{"http://github.com/a","https://github.com.evil.test/a","https://github.com@evil.test/a","https://release-assets.githubusercontent.com.evil.test/a","https://127.0.0.1/","https://github.com:80/a"})check(!ReleaseUpdate.allowedHttps(url),"Untrusted URL rejected");
        for(String url:new String[]{ReleaseUpdate.API,"https://github.com/MrMybal/CyInsights/releases/download/v9.0.0/a.apk","https://release-assets.githubusercontent.com/file?token=example"})check(ReleaseUpdate.allowedHttps(url),"Trusted HTTPS accepted");
        File file=File.createTempFile("cyinsights-update-check-",".bin");try{
            try(OutputStream out=new FileOutputStream(file)){out.write(new byte[]{1,2,3});}
            ReleaseUpdate.Candidate good=new ReleaseUpdate.Candidate("v9.0.0","test.apk","https://github.com/",ReleaseUpdate.hash(file),3);ReleaseUpdate.verify(file,good);++checks;
            reject(()->ReleaseUpdate.verify(file,new ReleaseUpdate.Candidate("v9.0.0","test.apk","https://github.com/",good.sha256,2)),"Wrong size rejected");
            reject(()->ReleaseUpdate.verify(file,new ReleaseUpdate.Candidate("v9.0.0","test.apk","https://github.com/",String.join("",java.util.Collections.nCopies(64,"0")),3)),"Wrong digest rejected");
        }finally{file.delete();}
        if(args.length>0&&args[0].equals("--network")){
            HttpsURLConnection c=(HttpsURLConnection)new URL(ReleaseUpdate.API).openConnection();c.setConnectTimeout(5000);c.setReadTimeout(10000);c.setRequestProperty("User-Agent","CyInsightOVR-Updater-Test");
            try{check(c.getResponseCode()==200,"Real GitHub request");try(InputStream in=c.getInputStream()){ByteArrayOutputStream text=new ByteArrayOutputStream();byte[] b=new byte[4096];int n;while((n=in.read(b))!=-1){if(text.size()+n>2*1024*1024)throw new IOException("Metadata too large");text.write(b,0,n);}JSONObject j=new JSONObject(text.toString("UTF-8"));check(ReleaseUpdate.select(j,"999999999.0.0","vulkan")==null,"Real metadata cannot downgrade");}}
            finally{c.disconnect();}
        }
        return checks;
    }
    public static void main(String[] args)throws Exception{System.out.println(run(args)+" Android GitHub update checks passed");}
}

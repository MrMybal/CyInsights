// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import org.json.*;
import java.io.*;
import java.util.regex.*;
import java.security.MessageDigest;
final class ReleaseUpdate {
    static final String API="https://api.github.com/repos/MrMybal/CyInsights/releases/latest";
    static final class Version {
        final long a,b,c;final boolean pre;
        Version(Matcher m){a=Long.parseLong(m.group(1));b=Long.parseLong(m.group(2));c=Long.parseLong(m.group(3));pre=m.group(4)!=null;}
        boolean newer(Version old){if(a!=old.a)return a>old.a;if(b!=old.b)return b>old.b;if(c!=old.c)return c>old.c;return !pre&&old.pre;}
    }
    static Version version(String text){Matcher m=Pattern.compile("^v?(0|[1-9][0-9]{0,8})\\.(0|[1-9][0-9]{0,8})\\.(0|[1-9][0-9]{0,8})(-[0-9A-Za-z]+(?:[.-][0-9A-Za-z]+)*)?$").matcher(text);return m.matches()?new Version(m):null;}
    static final class Candidate {
        final String tag,version,name,url,sha256;final long size;
        Candidate(String t,String n,String u,String h,long s){tag=t;version=t.startsWith("v")?t.substring(1):t;name=n;url=u;sha256=h;size=s;}
        JSONObject json()throws JSONException{return new JSONObject().put("tag",tag).put("name",name).put("url",url).put("sha256",sha256).put("size",size);}
    }
    static Candidate select(JSONObject j,String installed,String variant)throws Exception{
        Version current=version(installed);if(current==null)throw new IOException("Invalid installed version");
        if(!(j.opt("draft") instanceof Boolean)||!(j.opt("prerelease") instanceof Boolean)||j.getBoolean("draft")||j.getBoolean("prerelease"))return null;
        String tag=j.optString("tag_name");Version v=version(tag);if(v==null||v.pre||!v.newer(current))return null;
        if(!variant.equals("standard")&&!variant.equals("vulkan"))throw new IOException("Invalid APK variant");
        String name="CyInsights-"+tag+"-Quest-"+(variant.equals("vulkan")?"Vulkan":"Standard")+"-arm64.apk";
        String url="https://github.com/MrMybal/CyInsights/releases/download/"+tag+"/"+name;
        JSONArray assets=j.optJSONArray("assets");Candidate chosen=null;
        if(assets!=null)for(int i=0;i<assets.length();++i){JSONObject a=assets.getJSONObject(i);if(!name.equals(a.optString("name")))continue;
            if(chosen!=null)throw new IOException("Duplicate update asset");String digest=a.optString("digest");long size=a.optLong("size");
            if(!digest.matches("sha256:[0-9a-f]{64}")||!url.equals(a.optString("browser_download_url"))||size<=0||size>1024L*1024*1024)throw new IOException("Invalid GitHub update asset or SHA-256");
            if(!j.optString("html_url").equals("https://github.com/MrMybal/CyInsights/releases/tag/"+tag))throw new IOException("Unexpected GitHub release location");
            chosen=new Candidate(tag,name,url,digest.substring(7),size);
        }return chosen;
    }
    static boolean allowedHttps(String value){return value.matches("^https://(api\\.github\\.com|github\\.com|release-assets\\.githubusercontent\\.com|objects\\.githubusercontent\\.com)(/[^\\s#]*)?$");}
    static String hash(File f)throws Exception{
        MessageDigest d=MessageDigest.getInstance("SHA-256");try(InputStream in=new FileInputStream(f)){byte[] b=new byte[65536];int n;while((n=in.read(b))!=-1)d.update(b,0,n);}return hex(d.digest());
    }
    static String hex(byte[] b){StringBuilder out=new StringBuilder();for(byte v:b)out.append(String.format(java.util.Locale.ROOT,"%02x",v&255));return out.toString();}
    static void verify(File f,Candidate c)throws Exception{if(!f.isFile()||f.length()!=c.size||!hash(f).equals(c.sha256))throw new IOException("Update size or SHA-256 verification failed");}
}

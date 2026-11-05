// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;
import android.app.*;
import android.content.*;
import android.os.Bundle;
import android.view.*;
import android.webkit.WebView;
import java.util.concurrent.*;
import org.json.*;

/** Real packaged WebView smoke test. Does not start a game or a GPU capture. */
public final class HelpReaderProbe extends Instrumentation {
    private Activity activity;private WebView web;
    @Override public void onCreate(Bundle args){super.onCreate(args);start();}
    private WebView find(View view){if(view instanceof WebView)return (WebView)view;if(view instanceof ViewGroup){ViewGroup group=(ViewGroup)view;for(int i=0;i<group.getChildCount();i++){WebView child=find(group.getChildAt(i));if(child!=null)return child;}}return null;}
    private String evaluate(String code)throws Exception{
        CountDownLatch done=new CountDownLatch(1);String[] value={null};
        runOnMainSync(()->web.evaluateJavascript(code,result->{value[0]=result;done.countDown();}));
        if(!done.await(3,TimeUnit.SECONDS))throw new Exception("Local WebView did not answer");return value[0];
    }
    private void await(String expression)throws Exception{
        long end=System.currentTimeMillis()+30000;
        while(System.currentTimeMillis()<end){if("true".equals(evaluate(expression)))return;Thread.sleep(150);}
        throw new Exception("Local reader condition timed out: "+expression);
    }
    @Override public void onStart(){Bundle result=new Bundle();try{
        Intent intent=new Intent(getTargetContext(),HelpActivity.class).putExtra("page","timeline").putExtra("locale","en").addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        activity=startActivitySync(intent);runOnMainSync(()->web=find(activity.getWindow().getDecorView()));if(web==null)throw new Exception("Missing help WebView");
        await("document.documentElement.lang==='en' && !!document.querySelector('.cydoc-viewer') && !!document.getElementById('navigate')");
        evaluate("document.querySelector('[data-cydoc-module]')?.scrollIntoView({block:'center'})");
        evaluate("location.hash='/fr/timeline/navigate'");await("document.documentElement.lang==='fr' && !!document.getElementById('navigate')");
        evaluate("document.querySelector('[data-cydoc-module]')?.scrollIntoView({block:'center'})");
        await("[...document.images].some(i=>i.complete&&i.naturalWidth>0)");
        boolean[] safe={false};runOnMainSync(()->safe[0]=web.getSettings().getBlockNetworkLoads()&&!web.getSettings().getAllowUniversalAccessFromFileURLs());
        if(!safe[0])throw new Exception("Unsafe help WebView settings");
        result.putString("help_reader",evaluate("JSON.stringify({url:location.href,locale:document.documentElement.lang,images:document.images.length,imageDecoded:[...document.images].some(i=>i.complete&&i.naturalWidth>0),theme:document.querySelector('.cydoc-viewer').dataset.theme})"));
        result.putString("status","passed; real packaged file:// reader; no visual headset assertion");finish(Activity.RESULT_OK,result);
    }catch(Exception error){result.putString("error",error.toString());try{if(web!=null)result.putString("diagnostic",evaluate("JSON.stringify({url:location.href,ready:document.readyState,lang:document.documentElement.lang,text:document.body?.innerText?.slice(0,400),images:[...document.images].map(i=>({src:i.src,complete:i.complete,width:i.naturalWidth})),mediaError:document.querySelector('.media-error')?.textContent})"));}catch(Exception ignored){}finish(Activity.RESULT_CANCELED,result);}finally{if(activity!=null)runOnMainSync(()->activity.finish());}}
}

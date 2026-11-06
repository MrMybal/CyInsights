// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
package org.cyinsights.ovr;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.KeyEvent;
import android.webkit.*;
import android.widget.*;

/** Dedicated offline CyDoc reader; no JavaScript/native bridge is exposed. */
public final class HelpActivity extends Activity {
    private WebView reader;
    private Button back;
    private static final String BASE="file:///android_asset/Documentation/Documentation.html";
    static void open(Activity host,String page,String locale) {
        // Preserve the retired native manual's independent language preference.
        android.content.SharedPreferences prefs=host.getSharedPreferences("documentation",0);
        if(!prefs.contains("locale")){String previous="en";try{previous=new org.json.JSONObject(host.getPreferences(0).getString("display_settings","{}")).optString("manual_locale","en");}catch(Exception ignored){}prefs.edit().putString("locale","fr".equals(previous)?"fr":"en").apply();}
        host.startActivity(new Intent(host,HelpActivity.class).putExtra("page",page).putExtra("locale",locale));
    }
    private static boolean local(Uri uri) {
        return "file".equals(uri.getScheme())&&(uri.getAuthority()==null||uri.getAuthority().isEmpty())&&uri.getPath()!=null&&
            uri.getPath().startsWith("/android_asset/Documentation/")&&!uri.getPath().contains("..");
    }
    @Override public void onCreate(Bundle saved) {
        super.onCreate(saved);
        LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);
        back=new Button(this);String locale=getIntent().getStringExtra("locale");if(locale==null||locale.isEmpty())locale=getSharedPreferences("documentation",0).getString("locale","en");back.setText("fr".equals(locale)?"Retour à CyInsightOVR":"Back to CyInsightOVR");back.setOnClickListener(v->finish());root.addView(back);
        reader=new WebView(this);
        WebSettings s=reader.getSettings();s.setJavaScriptEnabled(true);s.setDomStorageEnabled(true);
        s.setAllowFileAccess(true);s.setAllowContentAccess(false);s.setAllowFileAccessFromFileURLs(false);s.setAllowUniversalAccessFromFileURLs(false);
        s.setBlockNetworkLoads(true);s.setSupportZoom(true);s.setBuiltInZoomControls(true);s.setDisplayZoomControls(false);
        reader.setWebChromeClient(new WebChromeClient(){@Override public boolean onConsoleMessage(ConsoleMessage msg){if(msg.messageLevel()==ConsoleMessage.MessageLevel.ERROR)android.util.Log.w("CyDoc",msg.message()+" ("+msg.lineNumber()+")");return true;}});
        reader.setWebViewClient(new WebViewClient(){
            @Override public boolean shouldOverrideUrlLoading(WebView view,WebResourceRequest req){return !local(req.getUrl());}
            @Override public boolean shouldOverrideUrlLoading(WebView view,String url){return !local(Uri.parse(url));}
            @Override public void onPageFinished(WebView view,String url){remember(url);}
            @Override public void doUpdateVisitedHistory(WebView view,String url,boolean reload){remember(url);}
        });
        root.addView(reader,new LinearLayout.LayoutParams(-1,0,1));setContentView(root);
        if(saved==null||reader.restoreState(saved)==null)openRoute(getIntent());
    }
    private void remember(String url){String fragment=Uri.parse(url).getFragment();if(fragment!=null&&fragment.startsWith("/fr/")){getSharedPreferences("documentation",0).edit().putString("locale","fr").apply();back.setText("Retour à CyInsightOVR");}else if(fragment!=null&&fragment.startsWith("/en/")){getSharedPreferences("documentation",0).edit().putString("locale","en").apply();back.setText("Back to CyInsightOVR");}}
    private void openRoute(Intent intent) {
        String page=intent.getStringExtra("page"),locale=intent.getStringExtra("locale");
        if(page==null||page.isEmpty())page="quest-setup";
        if(locale==null||locale.isEmpty())locale=getSharedPreferences("documentation",0).getString("locale","en");
        locale="fr".equals(locale)?"fr":"en";
        String route=BASE+"#/"+locale+"/"+Uri.encode(page);
        String anchor=intent.getStringExtra("anchor"),media=intent.getStringExtra("media");
        if(anchor!=null&&!anchor.isEmpty())route+="/"+Uri.encode(anchor);
        if(media!=null&&!media.isEmpty())route+="?media="+Uri.encode(media);
        reader.loadUrl(route);
    }
    @Override protected void onNewIntent(Intent intent){super.onNewIntent(intent);setIntent(intent);openRoute(intent);}
    @Override public boolean onKeyUp(int code,KeyEvent event){if(code==KeyEvent.KEYCODE_F1){openRoute(getIntent());return true;}return super.onKeyUp(code,event);}
    @Override public void onBackPressed(){if(reader.canGoBack())reader.goBack();else super.onBackPressed();}
    @Override protected void onSaveInstanceState(Bundle out){reader.saveState(out);super.onSaveInstanceState(out);}
    @Override protected void onDestroy(){reader.stopLoading();reader.destroy();super.onDestroy();}
}

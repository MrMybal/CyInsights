// Application-owned preferences and compatibility; the CyDoc reader is unmodified.
(()=>{
  try { if(!localStorage.getItem('cydoc:theme')) localStorage.setItem('cydoc:theme','nord'); } catch {}
  if(location.hash.startsWith('#manual:')) location.replace('#/en/'+encodeURIComponent(location.hash.slice(8)));
  const params=new URLSearchParams(location.hash.split('?')[1]||'');
  if(params.get('preferred')==='auto'){
    params.delete('preferred');let locale;
    try{locale=localStorage.getItem('cyinsights-help-locale');}catch{}
    const parts=location.hash.split('?')[0].split('/');
    if(locale==='fr'||locale==='en')parts[1]=locale;
    history.replaceState(null,'',parts.join('/')+(params.size?'?'+params:''));
  }
  function remember(){const locale=location.hash.split('/')[1];if(locale==='en'||locale==='fr')try{localStorage.setItem('cyinsights-help-locale',locale);}catch{}}
  addEventListener('hashchange',remember);remember();
})();

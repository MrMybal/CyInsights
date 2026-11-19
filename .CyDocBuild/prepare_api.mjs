// Use CyDoc's documented structured API importer, then organize the authoring
// Markdown as a tree. The exported reader is produced only by the official CLI.
import {importApi} from 'cydoc/api-import';
import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {dirname,resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {createHash} from 'node:crypto';
const base=dirname(fileURLToPath(import.meta.url)),root=resolve(base,'..');
const ops=JSON.parse(await readFile(resolve(root,'AI/app/operations.json'),'utf8')).operations;
const fr=JSON.parse(await readFile(resolve(base,'api-fr.json'),'utf8'));
const revisions=JSON.parse(await readFile(resolve(base,'api-revisions.json'),'utf8'));
const groups={
  'api-session':['get_state','open_trace','get_capabilities','get_metadata','focus_view','cancel_loading','get_analysis_guide'],
  'api-timing':['set_range','filter_function','list_threads','list_functions','hotspots','histogram','detect_issues','show_histogram','publish_finding','open_comparison','query_comparison','query_frames','query_scopes','query_relations'],
  'api-counters':['list_counters','load_counters','query_counters','query_frame_stats','pin_counter'],
  'api-activities':['load_tasks','query_tasks','query_task_links','load_activities','list_activity_lanes','query_activities'],
  'api-gpu':['query_gpu','query_vulkan_passes'],
  'api-capture':['capture_probe','capture_start','capture_gpu','capture_status','capture_stop']
};
const grouped=Object.values(groups).flat();
if(new Set(grouped).size!==ops.length||ops.some(op=>!grouped.includes(op.id)))throw Error('Review operation navigation coverage');
function translatedSchema(value,locale){
  if(Array.isArray(value))return value.map(item=>translatedSchema(item,locale));
  if(!value||typeof value!=='object')return value;
  return Object.fromEntries(Object.entries(value).map(([key,v])=>{
    if(key==='description'&&locale==='fr'){
      if(!fr.descriptions[v])throw Error('Missing French schema description: '+v);
      return [key,fr.descriptions[v]];
    }
    return [key,translatedSchema(v,locale)];
  }));
}
const canonical=ops.map(op=>({id:op.id,description:op.description,inputSchema:op.inputSchema,outputSchema:op.outputSchema}));
const fingerprint=createHash('sha256').update(JSON.stringify(canonical)).digest('hex');
if(fingerprint!==revisions.contractSha256)throw Error('API contract changed: review the French translation before updating api-revisions.json');
const metadata={};
for(const locale of ['en','fr']){
  const t=(en,frText)=>locale==='en'?en:frText;
  const symbols=ops.map(op=>{
    if(!fr.operations[op.id])throw Error('Missing French operation: '+op.id);
    const input=translatedSchema(op.inputSchema,locale),output=translatedSchema(op.outputSchema,locale);
    const description=t(op.description,fr.operations[op.id]);
    const parent=Object.keys(groups).find(group=>groups[group].includes(op.id));
    metadata[op.id]={parent,description};
    const permissions=op.permissions?.length?op.permissions.map(p=>'`'+p+'`').join(', '):t('Read-only; no extra operation permission.','Lecture seule ; aucune permission supplémentaire pour cette opération.');
    const body=description+'\n\n## '+t('Call contract','Contrat d’appel')+' {#call-contract}\n\n'+t('Call CyInsightsCli invoke OPERATION --pid PID --json JSON, the shared assistant registry, or native MCP. Results are structured application data. Use job_start/job_result for durable results; accepted actions may still be running.','Appelez CyInsightsCli invoke OPERATION --pid PID --json JSON, le registre de l’assistant ou MCP natif. Le resultat contient les donnees de l’application. Utilisez job_start/job_result pour les resultats durables ; une action acceptee peut encore etre en cours.')+'\n\n'+t('Permissions: ','Permissions : ')+permissions+'\n\n## '+t('Operation input','Entrée de l’opération')+' {#operation-input}\n\n```json\n'+JSON.stringify(input,null,2)+'\n```\n\n## '+t('Operation output','Sortie de l’opération')+' {#operation-output}\n\n```json\n'+JSON.stringify(output,null,2)+'\n```';
    const required=new Set(input.required||[]);
    return {key:op.id,name:op.id,language:'cpp',kind:'operation',
      signature:'CyInsightsCli invoke '+op.id+' --pid PID --params-file parameters.json',
      description:body,
      parameters:Object.entries(input.properties||{}).map(([name,schema])=>({name,type:schema.type||'object',optional:!required.has(name),default:schema.default===undefined?'':JSON.stringify(schema.default),description:schema.description||t('See the input schema for constraints and allowed values.','Voir le schéma d’entrée pour les contraintes et valeurs autorisées.')})),
      returnType:'JSON',returns:t('Structured native result. Accepted actions may still be loading; poll get_state/capture_status. job_result provides retained output for explicit background jobs.','Resultat natif structure. Les actions acceptees peuvent encore charger ; consulter get_state/capture_status. job_result fournit les sorties conservees des taches explicites.'),
      references:groups[parent].filter(id=>id!==op.id).slice(0,3).map(id=>({key:id,name:id}))};
  });
  const inputFile='work/api-'+locale+'.json';
  await mkdir(resolve(base,'work'),{recursive:true});
  await writeFile(resolve(base,inputFile),JSON.stringify({symbols}));
  const pages=await importApi([{format:'cydoc',path:inputFile,prefix:'operation',locale}],{base,locales:[{code:'en'},{code:'fr'}]});
  for(const page of pages){
    const meta=metadata[page.title];
    const frontmatter='---\nid: '+page.id+'\ntitle: '+page.title+'\ndescription: '+meta.description.replaceAll('\n',' ')+'\nsection: '+t('Reference / Developer API','Référence / API développeur')+'\nparent: '+meta.parent+'\norder: 100\nkind: api\nsymbol: '+page.title+'\n'+t('revision: ','sourceRevision: ')+fingerprint.slice(0,12)+'\n---\n\n';
    await mkdir(resolve(base,'manual',locale,'api'),{recursive:true});
    await writeFile(resolve(base,'manual',locale,'api',page.id+'.md'),frontmatter+page.markdown.trimEnd()+'\n');
  }
}
console.log('Prepared '+ops.length+' public operations in English and French with CyDoc structured import');

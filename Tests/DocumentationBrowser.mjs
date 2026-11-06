// Optional real-browser QA; playwright-core is a development-only dependency.
// node Tests/DocumentationBrowser.mjs <playwright-core/index.mjs> [browser executable]
import {pathToFileURL} from 'node:url';
import {resolve} from 'node:path';
import {mkdir,writeFile} from 'node:fs/promises';
import assert from 'node:assert/strict';
const {chromium}=await import(pathToFileURL(resolve(process.argv[2])).href);
const browser=await chromium.launch({headless:true,...(process.argv[3]?{executablePath:process.argv[3]}:{})});
const context=await browser.newContext({viewport:{width:1440,height:960},deviceScaleFactor:1});
await context.setOffline(true);
const errors=[],network=[];
context.on('request',request=>{if(/^https?:/.test(request.url()))network.push(request.url());});
const page=await context.newPage();page.on('pageerror',e=>errors.push(String(e)));
const directory=resolve('.CyDocBuild/work/qa');await mkdir(directory,{recursive:true});
const base=pathToFileURL(resolve('Documentation/Documentation.html')).href;
async function open(route=''){await page.goto(base+route);await page.locator('.cydoc-viewer').waitFor();await page.locator('main h1').waitFor();}
async function screenshot(name){await page.screenshot({path:resolve(directory,name+'.png')});}
try{
    await open();
    assert.equal(await page.locator('html').getAttribute('lang'),'en');
    assert.equal(await page.locator('main h1').innerText(),'Start here');
    const sidebar=await page.locator('.sidebar').boundingBox();assert.equal(sidebar.width,330);
    assert.ok(await page.locator('.header-search input').isVisible());
    assert.equal(await page.locator('.sidebar .side-card,.sidebar .sidebar-footer').count(),0);
    await page.waitForFunction(()=>document.querySelector('.brand-logo')?.naturalWidth>0);
    await page.locator('.brand').focus();await page.locator('[role=tooltip]:visible').waitFor();
    assert.ok((await page.locator('[role=tooltip]:visible').innerText()).includes('CyInsights & CyInsightOVR'));
    await page.keyboard.press('Escape');await screenshot('welcome-en');
    // Native details navigation, keyboard expansion and full-title/description tooltip.
    const desktop=page.locator('details:has(> summary > a[data-page="desktop"])');
    await desktop.locator(':scope > summary').press('ArrowRight');
    assert.ok(await desktop.getAttribute('open')!==null);
    await page.locator('.side-nav a[data-page="install-desktop"]').focus();
    await page.locator('[role=tooltip]:visible').waitFor();
    assert.ok((await page.locator('[role=tooltip]:visible').innerText()).includes('Windows package'));
    await page.keyboard.press('Escape');
    // Header search opens live results; Ctrl+K uses the same search dialog.
    await page.locator('.header-search input').fill('histogram');
    await page.locator('.search-result').first().waitFor();
    assert.ok((await page.locator('.search-dialog').innerText()).includes('Functions, callers and histograms'));
    await page.locator('.search-result[href="#/en/functions"]').first().click();
    await page.waitForURL(/#\/en\/functions$/);
    await page.keyboard.press('Control+k');await page.locator('dialog input[type=search]').fill('query_gpu');
    await page.locator('.search-result').first().waitFor();await page.keyboard.press('Escape');
    await open('#/en/timeline/navigate');
    const image=page.locator('[data-cydoc-module=image]').first();
    await image.scrollIntoViewIfNeeded();
    await page.waitForFunction(()=>document.querySelector('.cy-image img')?.naturalWidth>0);
    await image.hover();await image.locator('[data-action=expand]').click();
    assert.ok((await image.getAttribute('class')).includes('media-expanded'));
    const box=await image.boundingBox();assert.ok(box.x>=0&&box.y>=0&&box.width<=1440&&box.height<=960);
    assert.equal(await page.evaluate(()=>!!document.fullscreenElement),false);
    await screenshot('image-expanded');await page.keyboard.press('Escape');
    assert.ok(!(await image.getAttribute('class')).includes('media-expanded'));
    await page.locator('.locale-control select').selectOption('fr');
    await page.waitForFunction(()=>document.documentElement.lang==='fr');
    assert.equal(await page.locator('main h1').innerText(),'Naviguer et mesurer dans la timeline');
    await screenshot('timeline-fr');
    for(const theme of ['contrast','paper','nord']){
        await page.locator('.theme-picker summary').click();await page.locator('[data-set-theme="'+theme+'"]').click();
        assert.equal(await page.locator('.cydoc-viewer').getAttribute('data-theme'),theme);
    }
    // Long API page: return to its reading position rather than jumping to top.
    await open('#/en/operation-query-gpu-query-gpu');
    await page.mouse.move(1100,750);await page.mouse.wheel(0,950);await page.waitForTimeout(200);
    const oldScroll=await page.evaluate(()=>scrollY);assert.ok(oldScroll>500);
    await page.locator('.brand').click();await page.waitForURL(/#\/en\/welcome$/);
    await page.goBack();await page.waitForURL(/operation-query-gpu-query-gpu/);
    await page.waitForFunction(top=>Math.abs(scrollY-top)<3,oldScroll);
    assert.equal(await page.locator('main .source-code,.source-tree').count(),0);
    await open('#/fr/quest-heatmap/tile-grid');
    await page.locator('img[src*="tile-grid"]').scrollIntoViewIfNeeded();
    await page.waitForFunction(()=>[...document.images].some(i=>i.src.includes('tile-grid')&&i.naturalWidth>0));
    await page.setViewportSize({width:720,height:700});await screenshot('quest-narrow-fr');
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
    await open('#/fr/reference-docs-nativeactivities');
    await page.waitForFunction(()=>document.querySelector('main h1')?.textContent==='Commencer ici');
    await open('#/en/overview');assert.equal(await page.locator('main h1').innerText(),'Start here');
    const touch=await browser.newContext({viewport:{width:840,height:640},deviceScaleFactor:1.5,hasTouch:true});
    await touch.setOffline(true);const t=await touch.newPage();t.on('pageerror',e=>errors.push(String(e)));
    await t.goto(base+'#/fr/functions?media=histogram');await t.locator('.cy-image img').waitFor();
    await t.locator('.cy-image').scrollIntoViewIfNeeded();await t.locator('.cy-image [data-action=expand]').tap();
    assert.ok(await t.locator('.media-expanded').count());await t.screenshot({path:resolve(directory,'touch-dpi-150.png')});await touch.close();
    assert.deepEqual(errors,[]);assert.deepEqual(network,[]);
    await writeFile(resolve(directory,'result.json'),JSON.stringify({fileUrl:base,offline:true,externalRequests:0,pageErrors:0,languages:['en','fr'],sidebarWidth:330,passed:true},null,2));
    console.log('Real offline CyDoc browser checks passed: tree, search, tooltip, media, locales, scroll, themes, touch and DPI');
}catch(error){await screenshot('failure');await writeFile(resolve(directory,'failure.json'),JSON.stringify({url:page.url(),errors,network,body:(await page.locator('body').innerText()).slice(0,3000)},null,2));throw error;}finally{await browser.close();}

// Optional real-browser QA. npm install --prefix build/cydoc-browser playwright-core
// node Tests/DocumentationBrowser.mjs <path-to-playwright-core/index.mjs> [browser executable]
import { pathToFileURL } from 'node:url';
import { resolve } from 'node:path';
import { mkdir, writeFile } from 'node:fs/promises';
import assert from 'node:assert/strict';
const {chromium}=await import(pathToFileURL(resolve(process.argv[2])).href);
const browser=await chromium.launch({headless:true,...(process.argv[3]?{executablePath:process.argv[3]}:{})});
const context=await browser.newContext({viewport:{width:1360,height:900},deviceScaleFactor:1});
await context.setOffline(true);
const errors=[],network=[];
context.on('request',request=>{if(/^https?:/.test(request.url()))network.push(request.url());});
const page=await context.newPage();
page.on('pageerror',e=>errors.push(String(e)));
const directory=resolve('build/cydoc-qa');await mkdir(directory,{recursive:true});
const base=pathToFileURL(resolve('build/help/index.html')).href;
async function open(route){await page.goto(base+route);await page.locator('.cydoc-viewer').waitFor();await page.waitForFunction(()=>document.querySelectorAll('h1,h2').length>2);}
try{
    await open('#/en/timeline/navigate');
    await page.locator('[data-cydoc-module="image"] img').first().waitFor({state:'visible'});
    assert.equal(await page.locator('html').getAttribute('lang'),'en');
    assert.equal(await page.locator('.cydoc-viewer').getAttribute('data-theme'),'nord');
    await page.waitForTimeout(600); // Let CyDoc's anchor scroll settle before capture.
    await page.screenshot({path:resolve(directory,'timeline-en.png')});
    const image=page.locator('[data-cydoc-module="image"]').first();
    await image.evaluate(e=>e.scrollIntoView({block:'center',behavior:'instant'}));
    await image.hover();
    await image.locator('[data-action="expand"]').click();
    assert.ok((await image.getAttribute('class')).includes('media-expanded'));
    const box=await image.boundingBox();assert.ok(box.x>=0&&box.y>=0&&box.width<=1360&&box.height<=900);
    await page.screenshot({path:resolve(directory,'image-expanded.png')});
    await page.keyboard.press('Escape');assert.ok(!(await image.getAttribute('class')).includes('media-expanded'));
    await page.keyboard.press('Control+k');await page.locator('dialog input[type="search"]').fill('histogram');
    await page.locator('.search-result').first().waitFor();await page.keyboard.press('Escape');
    await page.locator('.locale-control select').selectOption('fr');
    await page.waitForFunction(()=>document.documentElement.lang==='fr');
    await page.locator('.locale-control select').selectOption('en');
    await page.waitForFunction(()=>document.documentElement.lang==='en');
    await page.locator('.theme-picker summary').click();await page.locator('[data-set-theme="contrast"]').click();
    assert.equal(await page.locator('.cydoc-viewer').getAttribute('data-theme'),'contrast');
    await page.locator('.theme-picker summary').click();await page.locator('[data-set-theme="nord"]').click();
    // Real link navigation and browser history, never a replacement renderer.
    await page.locator('a[href="#/en/functions"]').first().click();
    await page.waitForFunction(()=>location.hash==='#/en/functions',null,{timeout:5000});
    await page.goBack();await page.waitForFunction(()=>location.hash==='#/en/timeline/navigate',null,{timeout:5000});
    await open('#/fr/timeline/navigate');
    assert.equal(await page.locator('html').getAttribute('lang'),'fr');
    await page.screenshot({path:resolve(directory,'timeline-fr.png')});
    await open('#/en/quest-heatmap/tile-grid');
    assert.ok(await page.locator('img[src*="tile-grid"]').count());
    await page.setViewportSize({width:720,height:520});
    await page.waitForTimeout(600);
    await page.screenshot({path:resolve(directory,'quest-narrow.png')});
    await open('#/fr/reference-docs-nativeactivities');
    assert.ok((await page.locator('body').innerText()).includes('Technical reference'));
    await open('#/en/overview'); // legacy alias
    assert.ok((await page.locator('body').innerText()).includes('Choose your workflow'));
    await page.keyboard.press('Tab');
    assert.notEqual(await page.evaluate(()=>document.activeElement?.tagName),'BODY');
    const touch=await browser.newContext({viewport:{width:840,height:640},deviceScaleFactor:1.5,hasTouch:true});
    await touch.setOffline(true);const touchPage=await touch.newPage();
    touchPage.on('pageerror',e=>errors.push(String(e)));
    await touchPage.goto(base+'#/fr/functions?media=histogram');await touchPage.locator('.cy-image img').waitFor();
    await touchPage.locator('.cy-image').evaluate(e=>e.scrollIntoView({block:'center',behavior:'instant'}));
    await touchPage.locator('.cy-image [data-action="expand"]').tap();
    assert.ok(await touchPage.locator('.media-expanded').count());
    await touchPage.screenshot({path:resolve(directory,'touch-dpi-150.png')});await touch.close();
    assert.deepEqual(network,[]);assert.deepEqual(errors,[]);
    await writeFile(resolve(directory,'result.json'),JSON.stringify({fileUrl:base,offline:true,externalRequests:network.length,pageErrors:errors.length,passed:true},null,2));
    console.log('CyDoc actual file:// browser checks passed');
}catch(error){await page.screenshot({path:resolve(directory,'failure.png')});await writeFile(resolve(directory,'failure.json'),JSON.stringify({url:page.url(),errors,network,body:(await page.locator('body').innerText()).slice(0,3000)},null,2));throw error;}finally{await browser.close();}

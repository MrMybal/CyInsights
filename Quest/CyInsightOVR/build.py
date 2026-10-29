#!/usr/bin/env python3
# Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
"""Offline Android SDK/NDK build. Uses installed tooling, no Gradle downloads."""
import argparse, os, pathlib, shutil, subprocess, zipfile

ROOT=pathlib.Path(__file__).resolve().parents[2]
HERE=pathlib.Path(__file__).resolve().parent
def main():
    p=argparse.ArgumentParser();p.add_argument('--sdk',type=pathlib.Path,default=pathlib.Path(os.environ.get('ANDROID_HOME',str(pathlib.Path.home()/'AppData/Local/Android/Sdk'))));p.add_argument('--java',type=pathlib.Path,default=pathlib.Path(os.environ.get('JAVA_HOME','C:/Program Files/Android/Android Studio/jbr')));p.add_argument('--output',type=pathlib.Path,default=ROOT/'bin/CyInsightOVR');p.add_argument('--experimental-vulkan-layer',action='store_true');a=p.parse_args()
    sdk=a.sdk;out=a.output.resolve();work=ROOT/'build/CyInsightOVR';work.mkdir(parents=True,exist_ok=True);out.mkdir(parents=True,exist_ok=True)
    ndk=sdk/'ndk/27.2.12479018';cmake=sdk/'cmake/3.22.1/bin/cmake.exe';ninja=sdk/'cmake/3.22.1/bin/ninja.exe';bt=sdk/'build-tools/35.0.1';android=sdk/'platforms/android-34/android.jar'
    env=os.environ.copy();env['JAVA_HOME']=str(a.java);env['PATH']=str(a.java/'bin')+os.pathsep+env.get('PATH','')
    def run(*args):subprocess.run([str(x) for x in args],check=True,env=env,cwd=ROOT)
    if a.experimental_vulkan_layer:
        import struct
        shader_arrays=[]
        compiler=ndk/'shader-tools/windows-x86_64/glslc.exe'
        for symbol,source in [('Vert','heatmap.vert'),('Frag','heatmap.frag'),('Label','heatmap_label.frag')]:
            binary=work/(source+'.spv');run(compiler,'--target-env=vulkan1.1',HERE/'native'/source,'-o',binary)
            data=binary.read_bytes();values=struct.unpack('<'+'I'*(len(data)//4),data)
            shader_arrays.append('inline constexpr uint32_t '+symbol+'[]={'+','.join(hex(v) for v in values)+'};')
        (work/'native').mkdir(exist_ok=True)
        (work/'native/TileShaders.hpp').write_text('// Generated from our GLSL sources.\n#pragma once\n#include <cstdint>\nnamespace cytiles {\n'+'\n'.join(shader_arrays)+'\n}\n')
    run(cmake,'-S',HERE,'-B',work/'native','-G','Ninja',f'-DCMAKE_MAKE_PROGRAM={ninja}',f'-DCMAKE_TOOLCHAIN_FILE={ndk}/build/cmake/android.toolchain.cmake','-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-29','-DANDROID_STL=c++_static','-DCMAKE_BUILD_TYPE=Release','-DCYOVR_EXPERIMENTAL_VULKAN_LAYER='+('ON' if a.experimental_vulkan_layer else 'OFF'))
    run(cmake,'--build',work/'native','--parallel','6')
    # Package copies without local DWARF paths; preserve build symbols locally
    # and leave the pinned third-party OpenXR loader byte-for-byte unchanged.
    packaged=work/'packaged-libs';packaged.mkdir(exist_ok=True)
    native_libraries=['libcyinsightovr.so']
    if a.experimental_vulkan_layer:native_libraries.append('libVkLayer_CyInsightOVR.so')
    for name in native_libraries:
        shutil.copyfile(work/'native'/name,packaged/name)
        run(ndk/'toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-strip.exe','--strip-debug',packaged/name)
    classes=work/'classes';classes.mkdir(exist_ok=True)
    sources=sorted((HERE/'src').rglob('*.java'));response=work/'javac.args';response.write_text('\n'.join('"'+str(x).replace('\\','/')+'"' for x in sources),encoding='utf-8')
    run(a.java/'bin/javac.exe','-encoding','UTF-8','--release','8','-classpath',android,'-d',classes,'@'+str(response))
    jar=work/'classes.jar'
    with zipfile.ZipFile(jar,'w',zipfile.ZIP_DEFLATED) as z:
        for f in classes.rglob('*.class'):z.write(f,f.relative_to(classes).as_posix())
    dex=work/'dex';dex.mkdir(exist_ok=True)
    run(bt/'d8.bat','--release','--min-api','29','--lib',android,'--output',dex,jar)
    res=work/'res/drawable';res.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/'Assets/Branding/cyinsights-logo-ui-v1.png',res/'cyinsights_logo.png')
    run(bt/'aapt2.exe','compile','--dir',res.parent,'-o',work/'resources.zip')
    unsigned=work/'unsigned.apk';run(bt/'aapt2.exe','link','-o',unsigned,'--manifest',HERE/'AndroidManifest.xml','-I',android,work/'resources.zip')
    with zipfile.ZipFile(unsigned,'a',zipfile.ZIP_DEFLATED) as z:
        z.write(dex/'classes.dex','classes.dex');z.write(packaged/'libcyinsightovr.so','lib/arm64-v8a/libcyinsightovr.so')
        z.write(ROOT/'ThirdParty/OpenXR/Android/arm64-v8a/libopenxr_loader.so','lib/arm64-v8a/libopenxr_loader.so')
        if a.experimental_vulkan_layer:z.write(packaged/'libVkLayer_CyInsightOVR.so','lib/arm64-v8a/libVkLayer_CyInsightOVR.so')
        for folder in ['LICENCES']:
            for f in (ROOT/folder).rglob('*'):
                if f.is_file():z.write(f,'assets/'+f.relative_to(ROOT).as_posix())
        z.write(ROOT/'LICENSE','assets/LICENSE')
    aligned=work/'aligned.apk';run(bt/'zipalign.exe','-f','-p','4',unsigned,aligned)
    key=work/'development.keystore'
    if not key.exists():run(a.java/'bin/keytool.exe','-genkeypair','-keystore',key,'-storepass','android','-keypass','android','-alias','androiddebugkey','-dname','CN=CyInsightOVR Development','-keyalg','RSA','-keysize','2048','-validity','10000')
    apk=out/('CyInsightOVR-0.1.0-dev-vulkan-arm64.apk' if a.experimental_vulkan_layer else 'CyInsightOVR-0.1.0-dev-arm64.apk');run(bt/'apksigner.bat','sign','--ks',key,'--ks-key-alias','androiddebugkey','--ks-pass','pass:android','--key-pass','pass:android','--out',apk,aligned)
    run(bt/'apksigner.bat','verify','--verbose',apk)
    with zipfile.ZipFile(out/'cyinsightovr-bridge.jar','w',zipfile.ZIP_DEFLATED) as z:z.write(dex/'classes.dex','classes.dex')
    shutil.copyfile(HERE/'Start-CyInsightOVR.ps1',out/'Start-CyInsightOVR.ps1')
    shutil.copyfile(HERE/'README.md',out/'README.md')
    shutil.copyfile(HERE/'VALIDATION.md',out/'VALIDATION.md')
    print('Development APK:',apk)
if __name__=='__main__':main()

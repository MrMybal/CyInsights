"""Build our optional headless Quest GPU workload using an installed Android NDK.
Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: AGPL-3.0-or-later.
"""
import argparse
from pathlib import Path
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ndk',type=Path,required=True)
    args=parser.parse_args()
    ndk=args.ndk.resolve()
    output=ROOT/'build/quest-meta-probe'
    output.mkdir(parents=True,exist_ok=True)
    shaders={'MetaVertex':('vert','#version 450\nvoid main(){vec2 p[3]=vec2[](vec2(-1,-1),vec2(3,-1),vec2(-1,3));gl_Position=vec4(p[gl_VertexIndex],0,1);}\n'),
             'MetaFragment':('frag','#version 450\nlayout(location=0) out vec4 color;void main(){color=vec4(gl_FragCoord.xy/1536.0,0.25,1.0);}\n')}
    header='#pragma once\n#include <cstdint>\n'
    for name,(stage,code) in shaders.items():
        source=output/('probe.'+stage);source.write_text(code,encoding='utf-8')
        binary=output/(stage+'.spv')
        subprocess.run([str(ndk/'shader-tools/windows-x86_64/glslc.exe'),str(source),'-o',str(binary)],check=True)
        data=binary.read_bytes();words=struct.unpack('<'+'I'*(len(data)//4),data)
        header+='inline constexpr uint32_t '+name+'[]={'+','.join(hex(n) for n in words)+'};\n'
    (output/'MetaProbeShaders.h').write_text(header,encoding='utf-8')
    toolchain=ndk/'toolchains/llvm/prebuilt/windows-x86_64'
    subprocess.run([str(toolchain/'bin/clang++.exe'),'--target=aarch64-linux-android29','-std=c++17','-O2',
                    '-DCY_PERFETTO_WORKLOAD=1','-I'+str(output),str(ROOT/'Tools/QuestVulkanPassProbe.cpp'),
                    '-lvulkan','-static-libstdc++','-o',str(output/'quest-meta-workload')],check=True)
    print(output/'quest-meta-workload')

if __name__=='__main__':main()

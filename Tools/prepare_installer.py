"""Fetch the pinned, official portable NSIS compiler for package builds only.
Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: AGPL-3.0-or-later.
"""
from pathlib import Path
import hashlib
import urllib.request
from zipfile import ZipFile

ROOT=Path(__file__).resolve().parents[1]
VERSION='3.12'
SHA256='56581f90db321581c5381193d796fffcf2d24b2f8fed2160a6c6a3baa67f2c4f'
URL='https://downloads.sourceforge.net/project/nsis/NSIS%203/3.12/nsis-3.12.zip'

def main():
    output=ROOT/'build/packaging-tools'
    output.mkdir(parents=True,exist_ok=True)
    archive=output/f'nsis-{VERSION}.zip'
    if not archive.exists():
        request=urllib.request.Request(URL,headers={'User-Agent':'CyInsights-package-build'})
        with urllib.request.urlopen(request,timeout=45) as response:
            data=response.read(8*1024*1024)
        if hashlib.sha256(data).hexdigest()!=SHA256:raise SystemExit('NSIS download SHA-256 mismatch')
        archive.write_bytes(data)
    if hashlib.sha256(archive.read_bytes()).hexdigest()!=SHA256:raise SystemExit('NSIS archive SHA-256 mismatch')
    with ZipFile(archive) as package:
        for item in package.infolist():
            path=(output/item.filename).resolve()
            if not path.is_relative_to((output/f'nsis-{VERSION}').resolve()) or item.file_size>32*1024*1024:
                raise SystemExit('Unsafe compiler package entry')
        package.extractall(output)
    print(output/f'nsis-{VERSION}/makensis.exe')

if __name__=='__main__':main()

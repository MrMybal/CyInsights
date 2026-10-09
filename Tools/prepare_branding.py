"""Derive Windows/UI sizes from the approved logo; keep the master unchanged.

Requires Pillow only when regenerating assets, not to build/run the application.
"""
import hashlib
import json
from pathlib import Path
from PIL import Image

root=Path(__file__).resolve().parents[1]
assets=root/'Assets/Branding'
master=assets/'cyinsights-logo-v1.png'
with Image.open(master) as image:
    image=image.convert('RGBA')
    image.resize((512,512),Image.Resampling.LANCZOS).save(assets/'cyinsights-logo-ui-v1.png')
    image.save(assets/'cyinsights-logo-v1.ico',format='ICO',sizes=[(n,n) for n in (16,20,24,32,40,48,64,128,256)])
records={name:hashlib.sha256((assets/name).read_bytes()).hexdigest() for name in
    ('cyinsights-logo-v1.png','cyinsights-logo-ui-v1.png','cyinsights-logo-v1.ico')}
(assets/'branding-assets.json').write_text(json.dumps({'master':'cyinsights-logo-v1.png','icon_sizes':[16,20,24,32,40,48,64,128,256],'sha256':records},indent=2)+'\n',encoding='utf-8')
print(json.dumps(records))

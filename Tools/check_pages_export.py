"""Compatibility entry point for the documentation export gate."""
from pathlib import Path
import runpy
if __name__=='__main__':
    runpy.run_path(str(Path(__file__).resolve().parents[1]/'.CyDocBuild/check_export.py'),run_name='__main__')

"""Authenticated Windows-user IPC; no MCP/SDK protocol is reimplemented."""
import json
import os
import re
import time
from cytools_core import CyToolError

def native_call(operation, parameters, context=None):
    import pywintypes, win32file, win32event, win32pipe, win32con
    pid=os.environ.get('CYINSIGHTS_APP_PID','')
    if not re.fullmatch(r'[1-9][0-9]{0,9}',pid):
        raise CyToolError('MissingConnection','Set CYINSIGHTS_APP_PID from AI Inspector > Connection.',recoverable=True)
    pipe=r'\\.\pipe\CyInsights.AI.'+pid
    request=json.dumps({'operation':operation,'parameters':parameters},ensure_ascii=False,allow_nan=False).encode('utf-8')+b'\n'
    if len(request)>65536:raise CyToolError('InvalidParameters','Native request exceeds 64 KiB')
    def check():
        if context:context.cancellation.check()
    handle=None
    deadline=time.monotonic()+10
    while handle is None:
        check()
        try:handle=win32file.CreateFile(pipe,win32con.GENERIC_READ|win32con.GENERIC_WRITE,0,None,win32con.OPEN_EXISTING,win32con.FILE_FLAG_OVERLAPPED,None)
        except pywintypes.error as exc:
            if exc.winerror not in (2,231) or time.monotonic()>deadline:
                raise CyToolError('MissingConnection','Enable the local bridge in the selected CyInsights application.',recoverable=True) from exc
            if context:context.cancellation.sleep(.05)
            else:time.sleep(.05)
    def finish(overlapped):
        while win32event.WaitForSingleObject(overlapped.hEvent,25)==win32event.WAIT_TIMEOUT:check()
        check();return win32file.GetOverlappedResult(handle,overlapped,False)
    def io(write=None):
        ov=pywintypes.OVERLAPPED();ov.hEvent=win32event.CreateEvent(None,True,False,None)
        try:
            if write is None:
                error,buffer=win32file.ReadFile(handle,4096,ov);count=finish(ov)
                return bytes(buffer[:count])
            error,count=win32file.WriteFile(handle,write,ov);return finish(ov)
        finally:
            # The issuing thread owns these overlapped operations. Drain their
            # cancellation before releasing the event or Python read buffer.
            try:
                win32file.CancelIo(handle)
                try:win32file.GetOverlappedResult(handle,ov,True)
                except pywintypes.error:pass
            finally:win32file.CloseHandle(ov.hEvent)
    try:
        offset=0
        while offset<len(request):
            count=io(request[offset:])
            if not count:raise OSError('Disconnected while writing')
            offset+=count
        reply=bytearray()
        while b'\n' not in reply:
            check();part=io()
            if not part:raise OSError('Disconnected while reading')
            reply.extend(part)
            if len(reply)>4*1024**2:raise CyToolError('ResponseTooLarge','Use a smaller response limit')
        value=json.loads(reply.split(b'\n',1)[0])
        if isinstance(value.get('error'),dict):
            error=value['error'];raise CyToolError(error['code'],error['message'],recoverable=True)
        return value
    except (OSError,ValueError,pywintypes.error) as exc:
        raise CyToolError('ConnectionLost',str(exc),recoverable=True) from exc
    finally:
        try:win32file.CancelIo(handle)
        except pywintypes.error:pass
        win32file.CloseHandle(handle)

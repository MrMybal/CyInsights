"""Validate an already running, test-owned Unreal instance (IPv4/control port).

No engine launch, console execution, APK install or application launch is performed.
"""
import json
import socket
import sys
import threading
import time


def validate(host, port):
    checks = 0

    def check(condition, label):
        nonlocal checks
        if not condition:
            raise AssertionError(label)
        checks += 1

    def connect():
        client = socket.create_connection((host, port), 3)
        client.settimeout(3)
        return client, client.makefile("rb")

    def rpc(client, file, command, **fields):
        client.sendall((json.dumps(dict(version=1, command=command, **fields)) + "\n").encode())
        line = file.readline(65537)
        check(bool(line) and len(line) <= 65536, "bounded complete response")
        return json.loads(line)

    def listener(client):
        receiver = socket.socket()
        receiver.bind((client.getsockname()[0], 0))
        receiver.listen(1)
        receiver.settimeout(5)
        return receiver

    def drain(receiver, result):
        try:
            data, _ = receiver.accept()
            with data:
                data.settimeout(5)
                while True:
                    block = data.recv(65536)
                    if not block:
                        break
                    result.extend(block)
        except Exception as exc:
            result.extend(str(exc).encode())

    client, file = connect()
    with client, file:
        check(rpc(client, file, "hello")["ok"], "hello")
        check(not rpc(client, file, "console", text="quit")["ok"], "arbitrary command rejected")
        client.sendall(b'{"version":2,"command":"hello"}\n')
        check(not json.loads(file.readline())["ok"], "unsupported protocol rejected")
        check(not rpc(client, file, "start", receiver_port=-1)["ok"], "invalid port rejected")
        with listener(client) as receiver:
            raw = bytearray()
            thread = threading.Thread(target=drain, args=(receiver, raw))
            thread.start()
            start = rpc(client, file, "start", receiver_port=receiver.getsockname()[1], seconds=0)
            check(start["ok"] and bool(start["session_id"]), "manual remote start")
            check(not rpc(client, file, "start", receiver_port=receiver.getsockname()[1])["ok"], "overlap rejected")
            check(not rpc(client, file, "stop", session_id="wrong-session")["ok"], "wrong-session stop rejected")
            time.sleep(.2)
            check(rpc(client, file, "stop", session_id=start["session_id"])["ok"], "owned manual stop")
            thread.join(6)
            check(not thread.is_alive() and raw[:4] == b"2CRT", "native stream drains to EOF")
            check(rpc(client, file, "status")["state"] == "idle", "manual stop reaches idle")
        # Dropping the owning control connection must stop its network capture.
        with listener(client) as receiver:
            raw = bytearray()
            thread = threading.Thread(target=drain, args=(receiver, raw))
            thread.start()
            check(rpc(client, file, "start", receiver_port=receiver.getsockname()[1])["ok"], "repeat capture")
            time.sleep(.2)
            file.close()
            client.shutdown(socket.SHUT_RDWR)
            client.close()
            thread.join(6)
            check(not thread.is_alive() and raw[:4] == b"2CRT", "owner disconnect stops native writer")
    time.sleep(.1)
    client, file = connect()
    with client, file:
        check(rpc(client, file, "hello")["state"] == "idle", "reconnect after loss")
        with listener(client) as receiver:
            raw = bytearray()
            thread = threading.Thread(target=drain, args=(receiver, raw))
            thread.start()
            check(rpc(client, file, "start", receiver_port=receiver.getsockname()[1], seconds=.3)["ok"], "timed capture")
            thread.join(6)
            check(not thread.is_alive() and raw[:4] == b"2CRT", "timed stop drains native stream")
            final = rpc(client, file, "status")
            check(final["state"] == "idle" and final["manifest"]["session_id"] == final["session_id"], "final manifest matches session")
    print(f"{checks} remote Unreal checks passed")


if __name__ == "__main__":
    validate(sys.argv[1], int(sys.argv[2]))

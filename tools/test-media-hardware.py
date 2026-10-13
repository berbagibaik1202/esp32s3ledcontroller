"""Hardware integration test; join ESP32 AP first. Creates and removes test files."""
import json
import socket
import struct
import subprocess
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

http = urllib.request.build_opener(urllib.request.ProxyHandler({}))
prefix = "hwtest_" + str(int(time.time())) + "_"
created = []


def request(path, values=None, expected=200, raw=None, content_type=None):
    body = raw if raw is not None else (urllib.parse.urlencode(values).encode() if values is not None else None)
    req = urllib.request.Request("http://192.168.4.1" + path, data=body)
    if content_type:
        req.add_header("Content-Type", content_type)
    try:
        with http.open(req, timeout=60) as r:
            code, data = r.status, r.read()
    except urllib.error.HTTPError as e:
        code, data = e.code, e.read()
    assert code == expected, (path, code, data[:200])
    return data


def status():
    return json.loads(request("/api/status"))


def media():
    return json.loads(request("/api/media"))


def multipart(parts):
    boundary = "LedHardwareTestBoundary83019"
    body = bytearray()
    for name, data in parts:
        body.extend((f'--{boundary}\r\nContent-Disposition: form-data; name="file"; filename="{name}"\r\nContent-Type: application/octet-stream\r\n\r\n').encode())
        body.extend(data)
        body.extend(b"\r\n")
    body.extend(f"--{boundary}--\r\n".encode())
    return bytes(body), "multipart/form-data; boundary=" + boundary


def upload(name, data, expected=201):
    body, mime = multipart([(name, data)])
    result = request("/api/media/upload", raw=body, content_type=mime, expected=expected)
    if expected == 201:
        created.append("/media/" + name)
    return result


def control(command, **kwargs):
    return request("/api/control", {"command": command, **kwargs})


def reboot():
    import serial
    with serial.Serial("COM14", 115200, timeout=.2) as s:
        s.dtr = False
        s.rts = True
        time.sleep(.15)
        s.rts = False
        time.sleep(1)
    subprocess.run(["netsh", "wlan", "connect", "name=Codex-LED-Web-Test", "interface=Wi-Fi 2"], capture_output=True, timeout=15)
    end = time.monotonic() + 25
    while True:
        try:
            return status()
        except Exception:
            if time.monotonic() > end:
                raise
            time.sleep(1)


original = status()
bmp = Path("data/media/test_bars.bmp").read_bytes()
jpeg = Path("data/media/test_bars.jpg").read_bytes()
try:
    upload(prefix + "image.bmp", bmp)
    upload(prefix + "image.jpg", jpeg)
    paths = {f["path"] for f in media()["files"]}
    assert set(created) <= paths
    for path in created:
        control("file", path=path)
        assert status()["selected_media"] == path
        assert len(request("/api/frame")) > 4
    upload(prefix + "image.bmp", bmp, 409)
    upload(prefix + "broken.jpg", b"not a jpeg", 422)
    upload(prefix + "truncated.bmp", bmp[:-5], 422)
    upload(prefix + "empty.bmp", b"", 422)
    upload("../" + prefix + "escape.bmp", bmp, 400)
    wide = bytearray(bmp)
    struct.pack_into("<I", wide, 18, 129)
    upload(prefix + "wide.bmp", wide, 422)
    upload(prefix + "large.bmp", bytes(1048577), 413)
    body, mime = multipart([(prefix + "multi1.bmp", bmp), (prefix + "multi2.bmp", bmp)])
    request("/api/media/upload", raw=body, content_type=mime, expected=400)
    assert {f["path"] for f in media()["files"]} == paths
    request("/api/media/delete", {"path": "/media/../secret.bmp"}, 400)
    request("/api/media/delete", {"path": "/media/" + prefix + "missing.bmp"}, 404)
    print("PASS: BMP/JPEG upload/preview; duplicate, corrupt, truncated, empty, oversized, traversal, and multiple files rejected", flush=True)

    # Abruptly disconnect midway through a multipart file.
    body, mime = multipart([(prefix + "interrupted.bmp", bmp * 100)])
    with socket.create_connection(("192.168.4.1", 80), timeout=10) as sock:
        headers = f"POST /api/media/upload HTTP/1.1\r\nHost: 192.168.4.1\r\nContent-Type: {mime}\r\nContent-Length: {len(body)}\r\nConnection: close\r\n\r\n"
        sock.sendall(headers.encode() + body[:700])
    time.sleep(2)
    assert {f["path"] for f in media()["files"]} == paths
    upload(prefix + "after_abort.bmp", bmp)
    print("PASS: interrupted upload is hidden and next upload succeeds", flush=True)

    control("layout", preset="4")
    control("file", path=created[1])
    restored = reboot()
    assert restored["layout"] == 4 and restored["selected_media"] == created[1]
    assert (restored["width"], restored["height"]) == (128, 64)
    assert set(created) <= {f["path"] for f in media()["files"]}
    print("PASS: uploaded media, layout, and selected image survive hardware reset", flush=True)

    # Exercise the file-count capacity guard with small valid images.
    existing = len(media()["files"])
    for i in range(64 - existing):
        upload(prefix + f"capacity_{i}.bmp", bmp)
    upload(prefix + "over_capacity.bmp", bmp, 507)
    print("PASS: 64-file capacity returns 507 without removing existing files", flush=True)
finally:
    # Restore the user's original selection before removing only our test files.
    control("layout", preset=str(original["layout"]))
    if original.get("selected_media"):
        control("file", path=original["selected_media"])
    for path in created:
        request("/api/media/delete", {"path": path})
    assert not any(f["name"].startswith(prefix) for f in media()["files"])
    print("PASS: original display settings restored and all test files removed", flush=True)

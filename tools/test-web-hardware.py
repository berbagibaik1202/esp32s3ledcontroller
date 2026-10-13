"""Exercise the Web UI on a connected AP; run after joining ESP32 Wi-Fi."""
import json
import struct
import time
import urllib.error
import urllib.parse
import urllib.request

BASE = "http://192.168.4.1"
http = urllib.request.build_opener(urllib.request.ProxyHandler({}))


def request(path, values=None, expected=200):
    body = urllib.parse.urlencode(values).encode() if values is not None else None
    req = urllib.request.Request(BASE + path, data=body)
    try:
        with http.open(req, timeout=12) as response:
            code, data = response.status, response.read()
    except urllib.error.HTTPError as error:
        code, data = error.code, error.read()
    assert code == expected, (path, code, data[:200])
    return data


def status():
    return json.loads(request("/api/status"))


page = request("/").decode()
assert "LED Controller" in page and "wifiForm" in page
s = status()
assert s["memory_ok"] and s["storage_ok"] and s["ap_ready"]
assert "password" not in s
files = json.loads(request("/api/media"))["files"]
assert {f["name"] for f in files} >= {"test_bars.bmp", "test_bars.jpg"}
frame = request("/api/frame")
w, h = struct.unpack("<HH", frame[:4])
assert len(frame) == 4 + w * h * 2
for command in ["pattern", "bmp", "jpeg"]:
    before = status()["generation"]
    request("/api/control", {"command": command})
    assert status()["generation"] > before
for item in files:
    request("/api/control", {"command": "file", "path": item["path"]})
for preset, dimensions in [(4, (128, 64)), (0, (64, 32))]:
    request("/api/control", {"command": "layout", "preset": preset})
    s = status()
    assert (s["width"], s["height"]) == dimensions
request("/api/control", {"command": "layout", "preset": "garbage"}, 400)
request("/api/control", {"command": "file", "path": "/media/../secret.bmp"}, 422)
request("/api/wifi", {"ssid": "x" * 33, "password": ""}, 400)
request("/api/wifi", {"ssid": "test", "password": "short"}, 400)
request("/not-a-page", expected=404)
print("PASS: UI, status, RGB565 preview, media, controls, layouts, invalid requests", flush=True)

# Never replace a user's existing target during this test.
if not status()["target_ssid"]:
    try:
        request("/api/wifi", {"ssid": "LED-TEST-NOT-AVAILABLE-83019", "password": ""})
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            time.sleep(1)
            s = status()
            if s["uptime_ms"] and not s["sta_connecting"] and s["ap_ready"]:
                # Scan needs a few seconds to complete after deferred apply.
                if time.monotonic() > deadline - 12:
                    break
        assert s["ap_ready"] and not s["sta_connected"]
        assert s["target_ssid"] == "LED-TEST-NOT-AVAILABLE-83019"
        print("PASS: saved missing SSID keeps AP and HTTP available", flush=True)
    finally:
        request("/api/wifi", {"ssid": "", "password": ""})
        time.sleep(2)
    assert status()["target_ssid"] == ""
    print("PASS: restored AP-only configuration", flush=True)

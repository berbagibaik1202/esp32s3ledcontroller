"""Hardware trial tests; preserves the current committed scan and restores PC Wi-Fi."""
import json, re, subprocess, time, urllib.request, urllib.parse, urllib.error
http = urllib.request.build_opener(urllib.request.ProxyHandler({}))
def connect(name):
    subprocess.run(['netsh','wlan','connect','name='+name], capture_output=True, check=True)
def request(values=None, expected=200):
    body=urllib.parse.urlencode(values).encode() if values else None
    try:
        with http.open(urllib.request.Request('http://192.168.4.1/api/panel',data=body),timeout=2) as r:
            code,data=r.status,r.read()
    except urllib.error.HTTPError as e:
        code,data=e.code,e.read()
    assert code==expected,(code,data)
    return json.loads(data)
def wait_state(scan, preview, timeout=25):
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        try:
            state=request()
            if (scan is None or state['active_scan']==scan) and state['preview_active']==preview and not state['restart_pending']:
                return state
        except (OSError,ValueError): pass
        try: connect('ESP32S3-LED-Controller')
        except subprocess.CalledProcessError: pass
        time.sleep(.7)
    raise AssertionError('Timed out waiting for panel state')
status=subprocess.run(['netsh','wlan','show','interfaces'],capture_output=True,text=True,check=True).stdout
match=re.search(r'^\s*Profile\s*:\s*(.+)$',status,re.M)
previous=match.group(1).strip() if match else None
try:
    connect('ESP32S3-LED-Controller')
    initial=wait_state(None,False)
    baseline=initial['active_scan']; baseline_mapping=initial['active_mapping']
    candidate=16 if baseline!=16 else 32
    candidate_mapping=0 if candidate==16 else 4
    request({'command':'preview','scan':'3','mapping':'5'},400)
    assert request()['active_scan']==baseline
    request({'command':'confirm'},409)
    print('PASS: invalid scan and confirmation without preview rejected',flush=True)
    request({'command':'preview','scan':str(candidate),'mapping':str(candidate_mapping)})
    state=wait_state(candidate,True)
    assert 0<state['preview_remaining_ms']<=30000
    request({'command':'cancel'})
    wait_state(baseline,False)
    print('PASS: preview and cancel restore original scan',flush=True)
    request({'command':'preview','scan':str(candidate),'mapping':str(candidate_mapping)})
    wait_state(candidate,True)
    wait_state(baseline,False,timeout=45)
    print('PASS: unconfirmed preview automatically returns to original scan after timeout',flush=True)
    request({'command':'preview','scan':str(candidate),'mapping':str(candidate_mapping)})
    wait_state(candidate,True)
    import serial
    with serial.Serial('COM14',115200,timeout=.2) as port:
        port.dtr=False;port.rts=True;time.sleep(.15);port.rts=False
    wait_state(baseline,False)
    print('PASS: reset during preview restores committed scan',flush=True)
    request({'command':'preview','scan':str(baseline),'mapping':str(baseline_mapping)})
    wait_state(baseline,True)
    request({'command':'confirm'})
    assert not request()['preview_active']
    with serial.Serial('COM14',115200,timeout=.2) as port:
        port.dtr=False;port.rts=True;time.sleep(.15);port.rts=False
    wait_state(baseline,False)
    print('PASS: confirmed original scan persists across reset',flush=True)
finally:
    try:
        state=request()
        if state.get('preview_active'): request({'command':'cancel'})
    except Exception: pass
    if previous: connect(previous)

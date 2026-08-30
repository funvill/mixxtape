"""Fetch JLCPCB/LCSC parts into the mixxtape KiCad libraries via easyeda2kicad.

EasyEDA's API rejects plain urllib/curl clients (TLS fingerprinting), so this
driver monkeypatches urllib.request.urlopen with a curl_cffi Chrome
impersonation shim before invoking easyeda2kicad's CLI.

Usage:  python fetch_parts.py C701341 [C2758105 ...]
        python fetch_parts.py --all      # fetch the full BOM list below
"""

import sys
import urllib.error
import urllib.request
from pathlib import Path

from curl_cffi import requests as cffi_requests

LIB_BASE = str(Path(__file__).parent / "mixxtape_parts")

# LCSC IDs from docs/cassette-recorder-bom-v3.md
BOM_PARTS = [
    "C701341",   # U1  ESP32-WROOM-32E-N4
    "C2840615",  # U2  MSM261S4030H0R I2S MEMS mic
    "C2758105",  # U3  GD25Q128ESIG 16MB NOR flash (was C97521, Winbond)
    "C6186",     # U4  AMS1117-3.3 1A LDO (uprated per BOM note)
    "C506813",   # U6  CH340N USB-UART
    "C7519",     # U7  USBLC6-2SC6 USB ESD
    "C16581",    # U5  TP4056 charger (DNP)
    "C318884",   # SW1-4 tactile switch
    "C165948",   # J1  USB-C 16P
    "C2874073",  # D1-29 WS2812B-2020 (candidate — verify name in output)
    "C25905",    # R1,R2 5.1k CC pulldowns
    "C45783",    # C1,C2 22uF
    "C1525",     # 100nF
    "C15525",    # 10uF
    "C2150",     # Q1,Q2 S8050
    "C25804",    # 10k pullups
]


class _Resp:
    def __init__(self, r):
        self._r = r
        self.status = r.status_code
        self.headers = r.headers

    def read(self):
        return self._r.content

    def getcode(self):
        return self.status

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False


def _patched_urlopen(req, timeout=30, context=None, **kw):
    if isinstance(req, str):
        url, headers, data, method = req, {}, None, "GET"
    else:
        url = req.full_url
        headers = dict(req.header_items())
        data = req.data
        method = req.get_method()
    r = cffi_requests.request(
        method, url, headers=headers, data=data,
        impersonate="chrome", timeout=timeout,
    )
    if r.status_code >= 400:
        raise urllib.error.HTTPError(url, r.status_code, r.reason, hdrs=None, fp=None)
    return _Resp(r)


urllib.request.urlopen = _patched_urlopen

from easyeda2kicad.__main__ import main  # noqa: E402  (after the patch)

if __name__ == "__main__":
    args = sys.argv[1:]
    ids = BOM_PARTS if args == ["--all"] else args
    if not ids:
        print(__doc__)
        sys.exit(1)
    failed = []
    for lcsc_id in ids:
        print(f"=== {lcsc_id} ===")
        rc = main(["--full", "--lcsc_id", lcsc_id, "--output", LIB_BASE, "--overwrite"])
        if rc != 0:
            failed.append(lcsc_id)
    if failed:
        print(f"FAILED: {', '.join(failed)}")
    sys.exit(1 if failed else 0)

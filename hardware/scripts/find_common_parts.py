"""Search JLCPCB for parts meeting the project's supply-chain rule.

Rule (Steven, 2026-08-25): every fitted part must have >= 5,000 in stock,
and a common, boring part beats a better one that might vanish. A run of 20
boards is not worth a redesign because a clever chip went end-of-life.

    python find_common_parts.py "MEMS microphone" "I2S microphone" ...
    python find_common_parts.py --min 1 "microphone"   # survey what exists
"""

import argparse
import time

from curl_cffi import requests

URL = ("https://jlcpcb.com/api/overseas-pcb-order/v1/"
       "shoppingCart/smtGood/selectSmtComponentList")
MIN_STOCK = 5000


def search(keyword, pages=2):
    out = []
    for page in range(1, pages + 1):
        for attempt in range(4):
            try:
                time.sleep(1.0 + attempt * 1.5)
                r = requests.post(URL, json={
                    "currentPage": page, "pageSize": 25, "keyword": keyword,
                    "searchSource": "search", "firstSortName": "",
                    "secondSortName": "",
                }, impersonate="chrome", timeout=30)
                data = r.json().get("data")
                if not data:
                    continue
                lst = (data.get("componentPageInfo") or {}).get("list")
                if lst is None:
                    continue
                out.extend(lst)
                break
            except Exception:  # noqa: BLE001 - throttled, retry
                continue
    return out


def price_at(item, qty=20):
    for t in item.get("componentPrices") or []:
        end = t["endNumber"]
        if t["startNumber"] <= qty and (end < 0 or qty <= end):
            return t["productPrice"]
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--min", type=int, default=MIN_STOCK,
                    help="stock threshold (default: the project rule)")
    ap.add_argument("--top", type=int, default=25)
    ap.add_argument("keywords", nargs="+")
    args = ap.parse_args()

    seen = {}
    for kw in args.keywords:
        print(f"searching: {kw}")
        for it in search(kw):
            code = it.get("componentCode")
            if code and code not in seen:
                seen[code] = it

    rows = [it for it in seen.values()
            if (it.get("stockCount") or 0) >= args.min]
    rows.sort(key=lambda it: -(it.get("stockCount") or 0))
    rows = rows[:args.top]

    print(f"\n{len(rows)} of {len(seen)} candidates have >= {args.min:,} "
          f"in stock:\n")
    print(f"  {'LCSC':<12} {'stock':>9}  {'lib':<6} {'price':>9}  part")
    for it in rows:
        p = price_at(it)
        print(f"  {it.get('componentCode'):<12} "
              f"{it.get('stockCount'):>9,}  "
              f"{('basic' if it.get('componentLibraryType') == 'base' else 'ext'):<6} "
              f"{('$%.4f' % p) if p else '—':>9}  "
              f"{(it.get('componentModelEn') or '')[:30]} "
              f"[{(it.get('componentBrandEn') or '')[:18]}]")


if __name__ == "__main__":
    main()

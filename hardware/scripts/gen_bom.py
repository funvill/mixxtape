"""Generate the BOM directly from the schematic, with live JLCPCB data.

BOM v3 was hand-maintained and drifted badly from the schematic — it still
listed a USB-UART bridge and ESD diode that had been deleted, and was
missing the LED part actually fitted, the level shifter, the LDO change and
the microSD footprint. Deriving it from `mixxtape.kicad_sch` removes that
whole class of mistake: the BOM cannot disagree with the board any more.

Prices, stock and library type come from JLCPCB's parts API at run time, so
the numbers are what you would actually pay today rather than what someone
wrote down months ago. NOR flash pricing in particular has been moving.

    python gen_bom.py [--boards 20] [--out ../../docs/cassette-recorder-bom-v4.md]

Writes the markdown BOM and a JLCPCB-format assembly CSV next to it.
"""

import argparse
import json
import re
import sys
import time
from pathlib import Path

try:
    from curl_cffi import requests
except ImportError:
    sys.exit("needs curl_cffi (pip install curl_cffi) — JLC blocks plain clients")

HERE = Path(__file__).resolve().parent
SCH = HERE.parent / "mixxtape.kicad_sch"

JLC_SEARCH = ("https://jlcpcb.com/api/overseas-pcb-order/v1/"
              "shoppingCart/smtGood/selectSmtComponentList")

# JLCPCB charges a one-off feeder/reel setup fee per extended part per order.
EXTENDED_PART_FEE = 3.00

# Supply-chain rule (Steven, 2026-08-25): every fitted part must have at
# least this much stock at JLCPCB, and a common boring part beats a better
# one that might vanish. A run of 20 boards is not worth a redesign because
# a clever chip went end-of-life. Exceptions have to be argued for in
# STOCK_EXCEPTIONS below, not waved through.
MIN_STOCK = 5000

STOCK_EXCEPTIONS = {
    "C51928215": (
        "No digital MEMS microphone at JLCPCB meets the 5,000 rule - the "
        "whole category is thin, and the best-stocked digital part is only "
        "~3.5k. The one microphone that does clear the bar (ZTS6216, "
        "C481302, 29k) is ANALOG: it needs a DC-blocking cap and a gain "
        "stage into the ESP32's ADC, which the brief already describes as a "
        "lo-fi path with ~9 effective bits. Meeting the rule there would "
        "wreck the product's only input. A digital part at ~3.5k is the "
        "lesser risk: the run needs 20 pieces, so that is 174x coverage, "
        "and three alternates are qualified."
    ),
}

NOTES = {
    "C51928215": (
        "Replaces MSM261S4030H0R (C2840615), which went to zero stock. "
        "Acoustically an equal swap, not a downgrade: identical -26 dBFS "
        "sensitivity, same 1.6-3.6 V range, same 4x3 mm LGA-8 outline, 1 dB "
        "less SNR (60 vs 61 dB) and 2 dB better acoustic overload (122 vs "
        "120 dB SPL), which is the more useful end for a recorder that will "
        "meet a door slam. Cheaper too, $0.45 against $1.61, and lower "
        "current. Its one real cost is the interface: PDM rather than I2S, "
        "so firmware runs the ESP32's I2S peripheral in PDM RX mode - "
        "supported in hardware, and it frees GPIO26 because PDM needs two "
        "wires instead of three. Note the pinout is COMPLETELY different "
        "from the old part (pin 1 is VDD, not GND; pins 5-8 are all GND), "
        "so this was a schematic change, not a part-number swap. Like its "
        "predecessor it is TOP-ported: no hole through the board, keep its "
        "top face clear. Upgrade path if prototypes show the mic is the "
        "weak link: ICS-43434 (C5656610, ~3.7k, $3.33) keeps I2S and gains "
        "5 dB of SNR, at 7x the price and a different 3.5x2.65 mm "
        "footprint."
    ),
    "C97521": (
        "$2.26 here against $1.65 in BOM v3 — the NOR price rise the brief "
        "warned about is real. Second source C113767."
    ),
    "C45783": (
        "Basic part, so no setup fee, but $0.33 each is dear for a 22 uF "
        "0805. C5674 is $0.11 but extended: 40 pieces costs $13.00 as-is "
        "versus $4.47 + $3.00 fee. Worth ~$5.50 if you want it."
    ),
    "C5349955": (
        "Fitted in place of the genuine Worldsemi WS2812B-2020 (C965555), "
        "which is end-of-life at JLC with single-digit stock. Same protocol "
        "and land pattern; Steven has used these before."
    ),
    "C701341": (
        "N4 (4 MB). N16 is C701343 — take it if the delta is small, though "
        "the firmware fits in 4 MB comfortably. Stock is the thinnest of "
        "any fitted part after the microphone, but this module is a locked "
        "decision: it is the only ESP32 with Classic Bluetooth, and without "
        "that there is no A2DP and no product."
    ),
    "C7484": (
        "TI part, 26k stock. UMW second-source C20617903 has 40k and costs "
        "a third as much if stock ever tightens. Both clear the rule easily."
    ),
}


# ---------------------------------------------------------------- schematic

def tokenize(text):
    return re.findall(r'\(|\)|"(?:[^"\\]|\\.)*"|[^\s()"]+', text)


def parse(tokens, i=0):
    node = []
    i += 1
    while tokens[i] != ")":
        if tokens[i] == "(":
            child, i = parse(tokens, i)
            node.append(child)
        else:
            node.append(tokens[i])
            i += 1
    return node, i + 1


def unquote(s):
    return s.strip('"') if isinstance(s, str) else s


def read_components(path):
    """Returns a list of dicts: ref, value, footprint, lcsc, dnp."""
    tree, _ = parse(tokenize(path.read_text(encoding="utf-8")))
    out = []
    for node in tree:
        if not (isinstance(node, list) and node and node[0] == "symbol"):
            continue
        # Top-level placements have an `instances` block; lib_symbols do not.
        if not any(isinstance(x, list) and x and x[0] == "instances"
                   for x in node):
            continue
        comp = {"ref": "", "value": "", "footprint": "", "lcsc": "",
                "dnp": False}
        for el in node:
            if not isinstance(el, list):
                continue
            if el[0] == "dnp":
                comp["dnp"] = (el[1] == "yes")
            elif el[0] == "property":
                name, val = unquote(el[1]), unquote(el[2])
                if name == "Reference":
                    comp["ref"] = val
                elif name == "Value":
                    comp["value"] = val
                elif name == "Footprint":
                    comp["footprint"] = val
                elif name == "LCSC":
                    comp["lcsc"] = val
        # Power flags and other virtual symbols carry `in_bom no` and a
        # reference starting with '#'. They are ERC scaffolding, not parts.
        if comp["ref"].startswith("#"):
            continue
        if any(isinstance(x, list) and x and x[0] == "in_bom" and x[1] == "no"
               for x in node):
            continue
        if comp["ref"]:
            out.append(comp)
    return out


def ref_sort_key(ref):
    m = re.match(r"([A-Za-z]+)(\d+)", ref)
    return (m.group(1), int(m.group(2))) if m else (ref, 0)


# ---------------------------------------------------------------- JLCPCB

CACHE = HERE / ".jlc_cache.json"


def load_cache():
    if CACHE.exists():
        try:
            return json.loads(CACHE.read_text(encoding="utf-8"))
        except ValueError:
            pass
    return {}


def jlc_lookup(lcsc_id, cache):
    """Live price/stock for one LCSC part, or None.

    JLC throttles bursts, so requests are paced and retried. Results are
    cached so re-running after a schematic tweak does not re-hammer the API
    (delete .jlc_cache.json to force fresh prices).
    """
    if lcsc_id in cache:
        return cache[lcsc_id]

    for attempt in range(4):
        try:
            time.sleep(0.7 + attempt * 1.5)
            r = requests.post(JLC_SEARCH, json={
                "currentPage": 1, "pageSize": 25, "keyword": lcsc_id,
                "searchSource": "search", "firstSortName": "",
                "secondSortName": "",
            }, impersonate="chrome", timeout=30)
            items = (r.json().get("data") or {}).get(
                "componentPageInfo", {}).get("list", [])
        except Exception:  # noqa: BLE001 - throttled; back off and retry
            continue

        for it in items:
            if (it.get("componentCode") or "").upper() == lcsc_id.upper():
                cache[lcsc_id] = it
                return it
        cache[lcsc_id] = None  # searched cleanly, genuinely absent
        return None

    print(f"  ! {lcsc_id}: lookup failed after retries")
    return None


def price_at(item, qty):
    """Unit price at the tier covering `qty`, or None."""
    for tier in item.get("componentPrices") or []:
        end = tier["endNumber"]
        if tier["startNumber"] <= qty and (end < 0 or qty <= end):
            return tier["productPrice"]
    tiers = item.get("componentPrices") or []
    return tiers[-1]["productPrice"] if tiers else None


# ---------------------------------------------------------------- report

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--boards", type=int, default=20)
    ap.add_argument("--out", default=str(
        HERE.parent.parent / "docs" / "cassette-recorder-bom-v4.md"))
    args = ap.parse_args()

    comps = read_components(SCH)
    print(f"{len(comps)} placements in {SCH.name}")

    # Group by part number, not by value: the four buttons are one line
    # item even though their values name what each one does. DNP parts are
    # kept separate because they are not fitted on the default build.
    groups = {}
    for c in comps:
        key = (c["lcsc"] or f"~{c['value']}", c["dnp"])
        groups.setdefault(key, []).append(c)

    cache = load_cache()
    rows = []
    for (key, dnp), members in groups.items():
        lcsc = members[0]["lcsc"]
        seen = []
        for m in members:
            if m["value"] not in seen:
                seen.append(m["value"])
        value = " / ".join(seen)
        refs = sorted((m["ref"] for m in members), key=ref_sort_key)
        per_board = len(refs)
        order_qty = per_board * args.boards
        item = jlc_lookup(lcsc, cache) if lcsc else None
        unit = price_at(item, order_qty) if item else None
        rows.append({
            "refs": refs, "lcsc": lcsc, "value": value, "dnp": dnp,
            "per_board": per_board, "order_qty": order_qty,
            "footprint": members[0]["footprint"].split(":")[-1],
            "name": (item or {}).get("componentModelEn", ""),
            "brand": (item or {}).get("componentBrandEn", ""),
            "stock": (item or {}).get("stockCount"),
            "lib": (item or {}).get("componentLibraryType", ""),
            "unit": unit,
            "ext": (unit * order_qty) if unit else None,
            "found": item is not None,
        })
        status = "ok" if item else ("no LCSC" if not lcsc else "NOT FOUND")
        print(f"  {lcsc or '-':<12} {value:<24} x{per_board:<3} {status}")

    CACHE.write_text(json.dumps(cache), encoding="utf-8")
    rows.sort(key=lambda r: ref_sort_key(r["refs"][0]))
    write_markdown(rows, args)
    write_csv(rows, Path(args.out).with_suffix(".csv"))


def write_markdown(rows, args):
    boards = args.boards
    fitted = [r for r in rows if not r["dnp"]]
    dnp = [r for r in rows if r["dnp"]]

    parts_total = sum(r["ext"] for r in fitted if r["ext"])
    extended = [r for r in fitted if r["lib"] == "expand"]
    fees = len(extended) * EXTENDED_PART_FEE
    placements = sum(r["per_board"] for r in fitted)

    L = []
    A = L.append
    A(f"# Bill of materials — v4\n")
    A("> Generated from `hardware/mixxtape.kicad_sch` by")
    A("> `hardware/scripts/gen_bom.py`, with prices and stock read live from")
    A("> JLCPCB at the time shown. **Do not hand-edit** — change the")
    A("> schematic and re-run, so the two can never disagree again (v3 did,")
    A("> in seven places).\n")
    A(f"Quantities are for a run of **{boards} boards**, and unit prices are")
    A("taken from the tier covering the whole order rather than a single")
    A("piece.\n")
    A("---\n")
    A("## Fitted parts\n")
    A("| Ref | Value | LCSC | Package | /bd | Order qty | Stock | Lib | Unit | Ext |")
    A("|---|---|---|---|---|---|---|---|---|---|")
    for r in fitted:
        refs = ", ".join(r["refs"])
        if len(refs) > 42:
            refs = f"{r['refs'][0]}–{r['refs'][-1]} ({r['per_board']})"
        stock = "—" if r["stock"] is None else f"{r['stock']:,}"
        short = {"base": "basic", "expand": "ext"}.get(r["lib"], r["lib"] or "?")
        unit = f"${r['unit']:.4f}" if r["unit"] else "—"
        ext = f"${r['ext']:.2f}" if r["ext"] else "—"
        warn = " ⚠" if (r["stock"] is not None
                        and r["stock"] < r["order_qty"]) else ""
        A(f"| {refs} | {r['value']} | {r['lcsc'] or '—'} | {r['footprint']} "
          f"| {r['per_board']} | {r['order_qty']} | {stock}{warn} | {short} "
          f"| {unit} | {ext} |")

    A("")
    A("⚠ marks a part whose current JLCPCB stock is below what this run")
    A("needs — check before ordering, or pick the second source.\n")

    A("## Not fitted (DNP)\n")
    A("Footprints on the board, deliberately unpopulated. They cost nothing")
    A("on the default build and let a variant be built without a respin.\n")
    A("| Ref | Value | LCSC | Package | Purpose |")
    A("|---|---|---|---|---|")
    purpose = {
        "U5": "Battery charger — USB-C-only is a locked decision",
        "J2": "LiPo connector",
        "R8": "Charge-current programming resistor",
        "J4": "microSD — the \"studio edition\" option",
        "R9": "microSD chip-select pull-up",
    }
    for r in dnp:
        for ref in r["refs"]:
            A(f"| {ref} | {r['value']} | {r['lcsc'] or '—'} | {r['footprint']} "
              f"| {purpose.get(ref, '')} |")

    A("")
    A("## Cost\n")
    A(f"| | Per board | Run of {boards} |")
    A("|---|---|---|")
    A(f"| Components | ${parts_total / boards:.2f} | ${parts_total:.2f} |")
    A(f"| Extended-part setup fees ({len(extended)} × ${EXTENDED_PART_FEE:.2f}) "
      f"| ${fees / boards:.2f} | ${fees:.2f} |")
    A(f"| **Parts subtotal** | **${(parts_total + fees) / boards:.2f}** "
      f"| **${parts_total + fees:.2f}** |")
    A("")
    A(f"{placements} placements per board across {len(fitted)} distinct fitted")
    A(f"parts, {len(extended)} of them extended.\n")
    A("PCB fabrication and assembly labour are quoted separately and are not")
    A("included above — at this quantity the setup fees above dominate the")
    A("component cost, so adding a distinct extended part is expensive and")
    A("adding more of one you already use is nearly free.\n")

    attention = [(r, NOTES[r["lcsc"]]) for r in rows if r["lcsc"] in NOTES]
    short = [r for r in rows
             if not r["dnp"] and r["stock"] is not None
             and r["stock"] < r["order_qty"]]

    below = [r for r in rows if not r["dnp"]
             and r["stock"] is not None and r["stock"] < MIN_STOCK]
    A("## Supply-chain rule: every fitted part needs "
      f"{MIN_STOCK:,}+ in stock\n")
    if not below:
        A("All fitted parts clear the rule.\n")
    for r in below:
        note = STOCK_EXCEPTIONS.get(r["lcsc"])
        state = "accepted exception" if note else "**VIOLATION**"
        A(f"- `{r['lcsc']}` {r['value']} — {r['stock']:,} in stock, needs "
          f"{r['order_qty']} — {state}")
        if note:
            A("")
            A(f"  {note}")
        A("")

    A("## Needs attention before ordering\n")
    if short:
        A("**Insufficient stock for this run:**\n")
        for r in short:
            A(f"- `{r['lcsc']}` {r['value']} — needs {r['order_qty']}, "
              f"JLC shows {r['stock']:,}")
        A("")
    for r, note in attention:
        head = ", ".join(r["refs"][:3]) + ("…" if len(r["refs"]) > 3 else "")
        A(f"**{head} — {r['value']} (`{r['lcsc']}`)**")
        A("")
        A(note)
        A("")

    A("## Second sources\n")
    A("| Part | Primary | Alternate | Note |")
    A("|---|---|---|---|")
    A("| Audio flash | C97521 | C113767 | Same W25Q128JV, different vendor |")
    A("| ESP32 module | C701341 (N4) | C701343 (N16) | 16 MB module if the "
      "delta is small; firmware fits in 4 MB |")
    A("| Reel LEDs | C5349955 | C5349956 (SK6812 protocol) | Genuine "
      "WS2812B-2020 (C965555) is end-of-life at JLC |")
    A("| Level shifter | C7484 | C20617900 (UMW) | Cheaper, less known |")
    A("")

    out = Path(args.out)
    out.write_text("\n".join(L), encoding="utf-8", newline="\n")
    print(f"\nwrote {out}")
    print(f"  parts ${parts_total:.2f} + fees ${fees:.2f} "
          f"= ${parts_total + fees:.2f} for {boards} boards "
          f"(${(parts_total + fees) / boards:.2f}/board)")


def write_csv(rows, path):
    """JLCPCB assembly BOM format: Comment, Designator, Footprint, LCSC."""
    lines = ["Comment,Designator,Footprint,JLCPCB Part #"]
    for r in rows:
        if r["dnp"] or not r["lcsc"]:
            continue
        des = " ".join(r["refs"])
        lines.append(f'"{r["value"]}","{des}","{r["footprint"]}","{r["lcsc"]}"')
    path.write_text("\n".join(lines), encoding="utf-8", newline="\n")
    print(f"wrote {path} (fitted parts only)")


if __name__ == "__main__":
    main()

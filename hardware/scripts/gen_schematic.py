"""One-shot generator for the initial mixxtape schematic.

Reads symbol definitions from parts/mixxtape_parts.kicad_sym and
parts/mixxtape_local.kicad_sym, places every BOM component, and connects
pins with global labels according to the GPIO map in
docs/cassette-recorder-agent-brief.md.  Output: hardware/mixxtape.kicad_sch.

After the schematic has been hand-edited in KiCad, do NOT re-run this —
it overwrites the file.
"""

import re
import uuid
from pathlib import Path

HW = Path(__file__).resolve().parent.parent
PARTS_LIB = HW / "parts" / "mixxtape_parts.kicad_sym"
LOCAL_LIB = HW / "parts" / "mixxtape_local.kicad_sym"
OUT = HW / "mixxtape.kicad_sch"

G = 2.54  # placement grid


def u():
    return str(uuid.uuid4())


# ---------------------------------------------------------------- lib parsing

def tokenize(s):
    return re.findall(r'\(|\)|"(?:[^"\\]|\\.)*"|[^\s()"]+', s)


def parse(tokens, i=0):
    node = []
    i += 1  # skip (
    while tokens[i] != ")":
        if tokens[i] == "(":
            child, i = parse(tokens, i)
            node.append(child)
        else:
            node.append(tokens[i])
            i += 1
    return node, i + 1


def extract_symbol_blocks(text):
    """Return {name: raw_sexp_text} for each top-level symbol in a lib file."""
    blocks = {}
    for m in re.finditer(r'\(symbol\s+"([^"]+)"', text):
        # only top-level symbols (indentation of one tab in kicad output)
        start = m.start()
        line_start = text.rfind("\n", 0, start) + 1
        if text[line_start:start].strip():
            continue
        indent = start - line_start
        if indent > 2:  # nested unit symbol
            continue
        depth = 0
        i = start
        while True:
            if text[i] == "(":
                depth += 1
            elif text[i] == ")":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        blocks[m.group(1)] = text[start:i + 1]
    return blocks


def pin_positions(block_text):
    """Return {pin_number: (x, y, angle)} from a symbol block."""
    tree, _ = parse(tokenize(block_text))
    pins = {}
    for sub in tree:
        if isinstance(sub, list) and sub and sub[0] == "symbol":
            for el in sub:
                if isinstance(el, list) and el and el[0] == "pin":
                    at = num = None
                    for f in el:
                        if isinstance(f, list):
                            if f[0] == "at":
                                at = (float(f[1]), float(f[2]), int(float(f[3])))
                            elif f[0] == "number":
                                num = f[1].strip('"')
                    pins[num] = at
    return pins


parts_text = PARTS_LIB.read_text(encoding="utf-8")
local_text = LOCAL_LIB.read_text(encoding="utf-8")
parts_blocks = extract_symbol_blocks(parts_text)
local_blocks = extract_symbol_blocks(local_text)

LIBS = {}
for name, block in parts_blocks.items():
    LIBS[f"mixxtape_parts:{name}"] = block
for name, block in local_blocks.items():
    LIBS[f"mixxtape_local:{name}"] = block

PINS = {lib_id: pin_positions(block) for lib_id, block in LIBS.items()}

# ---------------------------------------------------------------- components

FP = "mixxtape_parts:"

# (ref, lib, value, footprint, lcsc, dnp, (x, y), {pin: net})
# net "NC" -> no_connect marker; net None -> leave untouched
COMPONENTS = [
    # --- USB front end ---
    ("J1", "mixxtape_parts:TYPE-C-31-M-12", "USB-C-16P", FP + "USB-C_SMD-TYPE-C-31-M-12_1", "C165948", False, (16, 26), {
        "A1B12": "GND", "B1A12": "GND", "A4B9": "VBUS", "B4A9": "VBUS",
        "A5": "CC1", "B5": "CC2",
        "A6": "USB_DP_C", "B6": "USB_DP_C", "A7": "USB_DN_C", "B7": "USB_DN_C",
        "A8": "NC", "B8": "NC", "1": "GND", "2": "GND", "3": "GND", "4": "GND"}),
    ("R1", "mixxtape_parts:0402WGF5101TCE", "5.1k", FP + "R0402", "C25905", False, (13, 40), {"1": "CC1", "2": "GND"}),
    ("R2", "mixxtape_parts:0402WGF5101TCE", "5.1k", FP + "R0402", "C25905", False, (20, 40), {"1": "CC2", "2": "GND"}),
    ("U7", "mixxtape_parts:USBLC6-2SC6", "USBLC6-2SC6", FP + "SOT-23-6_L2.9-W1.6-P0.95-LS2.8-BL", "C7519", False, (40, 26), {
        "1": "USB_DP_C", "6": "USB_DP", "3": "USB_DN_C", "4": "USB_DN",
        "2": "GND", "5": "VBUS"}),
    ("U6", "mixxtape_parts:CH340N", "CH340N", FP + "SOP-8_L5.0-W4.0-P1.27-LS6.0-BL", "C506813", False, (64, 26), {
        "1": "USB_DP", "2": "USB_DN", "3": "GND", "4": "RTS_N",
        "5": "VBUS", "6": "UART_RX", "7": "UART_TX", "8": "CH340_V3"}),
    ("C3", "mixxtape_parts:CL05B104KO5NNNC", "100nF", FP + "C0402", "C1525", False, (78, 40), {"1": "CH340_V3", "2": "GND"}),
    ("C4", "mixxtape_parts:CL05B104KO5NNNC", "100nF", FP + "C0402", "C1525", False, (64, 40), {"1": "VBUS", "2": "GND"}),

    # --- Power ---
    ("U4", "mixxtape_parts:AMS1117-3.3", "AMS1117-3.3", FP + "SOT-223-3_L6.5-W3.4-P2.30-LS7.0-BR", "C6186", False, (104, 22), {
        "1": "GND", "2": "3V3", "3": "VBUS", "4": "3V3"}),
    ("C1", "mixxtape_parts:CL21A226MAQNNNE", "22uF", FP + "C0805", "C45783", False, (92, 34), {"1": "VBUS", "2": "GND"}),
    ("C2", "mixxtape_parts:CL21A226MAQNNNE", "22uF", FP + "C0805", "C45783", False, (112, 34), {"1": "3V3", "2": "GND"}),

    # --- Battery option (all DNP) ---
    ("U5", "mixxtape_parts:TP4056", "TP4056 (DNP)", FP + "ESOP-8_L4.9-W3.9-P1.27-LS6.0-BL-EP", "C16581", True, (134, 24), {
        "1": "GND", "2": "PROG", "3": "GND", "4": "VBUS", "5": "VBAT",
        "6": "NC", "7": "NC", "8": "VBUS", "9": "GND"}),
    ("R8", "mixxtape_parts:0603WAF1002T5E", "10k (DNP)", FP + "R0603", "C25804", True, (134, 40), {"1": "PROG", "2": "GND"}),
    ("J2", "mixxtape_local:CONN_2", "JST-PH-2 (DNP)", "", "", True, (152, 24), {"1": "VBAT", "2": "GND"}),

    # --- ESP32 ---
    ("U1", "mixxtape_parts:ESP32-WROOM-32E", "ESP32-WROOM-32E-N4", FP + "WIFI-SMD_ESP32-WROOM-32E", "C701341", False, (64, 90), {
        "1": "GND", "2": "3V3", "3": "EN",
        "4": "NC", "5": "NC",
        "6": "NC",   # IO34: battery-sense divider is DNP
        "7": "NC",   # IO35: line-in is DNP
        "8": "NC",   # IO32 free
        "9": "I2S_SD", "10": "I2S_WS", "11": "I2S_BCLK", "12": "LED_DATA",
        "13": "GPIO14", "14": "NC",  # IO12 = MTDI strap, keep unused
        "15": "GND", "16": "GPIO13",
        "17": "NC", "18": "NC", "19": "NC", "20": "NC", "21": "NC", "22": "NC",
        "23": "NC", "24": "NC",  # IO15/IO2 free
        "25": "BTN_REC", "26": "BTN_PLAY", "27": "BTN_TRACK", "28": "BTN_MODE",
        "29": "SPI_CS_N", "30": "SPI_CLK", "31": "SPI_MISO",
        "32": "NC", "33": "TAB_SENSE", "34": "UART_RX", "35": "UART_TX",
        "36": "NC",  # IO22 free
        "37": "SPI_MOSI", "38": "GND", "39": "GND"}),
    ("R3", "mixxtape_parts:0603WAF1002T5E", "10k", FP + "R0603", "C25804", False, (24, 66), {"1": "3V3", "2": "EN"}),
    ("C9", "mixxtape_parts:CL05B104KO5NNNC", "100nF", FP + "C0402", "C1525", False, (24, 74), {"1": "EN", "2": "GND"}),
    ("C10", "mixxtape_parts:CL05B104KO5NNNC", "100nF", FP + "C0402", "C1525", False, (24, 58), {"1": "RTS_N", "2": "EN"}),
    ("C5", "mixxtape_parts:CL05A106MQ5NUNC", "10uF", FP + "C0402", "C15525", False, (70, 66), {"1": "3V3", "2": "GND"}),
    ("C6", "mixxtape_parts:CL05B104KO5NNNC", "100nF", FP + "C0402", "C1525", False, (78, 66), {"1": "3V3", "2": "GND"}),

    # --- Mic ---
    ("U2", "mixxtape_parts:MSM261S4030H0R", "MSM261S4030H0R", FP + "MIC-SMD_8P-L4.0-W3.0-P1.00-BR", "C2840615", False, (12, 104), {
        "1": "GND", "2": "NC", "3": "I2S_WS", "4": "3V3",
        "5": "GND", "6": "I2S_BCLK", "7": "I2S_SD", "8": "3V3"}),
    ("C7", "mixxtape_parts:CL05B104KO5NNNC", "100nF", FP + "C0402", "C1525", False, (12, 112), {"1": "3V3", "2": "GND"}),

    # --- Audio flash ---
    ("U3", "mixxtape_parts:W25Q128JVSIQTR", "W25Q128JVSIQ", FP + "SOIC-8_L5.3-W5.3-P1.27-LS8.0-BL", "C97521", False, (104, 96), {
        "1": "SPI_CS_N", "2": "SPI_MISO", "3": "FLASH_WP_N", "4": "GND",
        "5": "SPI_MOSI", "6": "SPI_CLK", "7": "FLASH_HOLD_N", "8": "3V3"}),
    ("R4", "mixxtape_parts:0603WAF1002T5E", "10k", FP + "R0603", "C25804", False, (94, 88), {"1": "3V3", "2": "SPI_CS_N"}),
    ("R5", "mixxtape_parts:0603WAF1002T5E", "10k", FP + "R0603", "C25804", False, (100, 88), {"1": "3V3", "2": "FLASH_WP_N"}),
    ("R6", "mixxtape_parts:0603WAF1002T5E", "10k", FP + "R0603", "C25804", False, (106, 88), {"1": "3V3", "2": "FLASH_HOLD_N"}),
    ("C8", "mixxtape_parts:CL05B104KO5NNNC", "100nF", FP + "C0402", "C1525", False, (112, 96), {"1": "3V3", "2": "GND"}),

    # --- Controls ---
    ("SW1", "mixxtape_parts:TS-1187A-B-A-B", "REC", FP + "SW-SMD_4P-L5.1-W5.1-P3.70-LS6.5-TL_H1.5", "C318884", False, (32, 108), {
        "1": "BTN_REC", "2": "BTN_REC", "3": "GND", "4": "GND"}),
    ("SW2", "mixxtape_parts:TS-1187A-B-A-B", "PLAY", FP + "SW-SMD_4P-L5.1-W5.1-P3.70-LS6.5-TL_H1.5", "C318884", False, (44, 108), {
        "1": "BTN_PLAY", "2": "BTN_PLAY", "3": "GND", "4": "GND"}),
    ("SW3", "mixxtape_parts:TS-1187A-B-A-B", "TRACK", FP + "SW-SMD_4P-L5.1-W5.1-P3.70-LS6.5-TL_H1.5", "C318884", False, (56, 108), {
        "1": "BTN_TRACK", "2": "BTN_TRACK", "3": "GND", "4": "GND"}),
    ("SW4", "mixxtape_parts:TS-1187A-B-A-B", "MODE", FP + "SW-SMD_4P-L5.1-W5.1-P3.70-LS6.5-TL_H1.5", "C318884", False, (68, 108), {
        "1": "BTN_MODE", "2": "BTN_MODE", "3": "GND", "4": "GND"}),
    ("TAB1", "mixxtape_local:BREAKOFF_TAB", "WRITE-PROTECT", "", "", False, (84, 108), {
        "1": "3V3", "2": "TAB_SENSE"}),
    ("R7", "mixxtape_parts:0603WAF1002T5E", "10k", FP + "R0603", "C25804", False, (84, 113), {"1": "TAB_SENSE", "2": "GND"}),

    # --- Hacker header ---
    ("J3", "mixxtape_local:CONN_10", "CASTELLATED", "", "", False, (132, 88), {
        "1": "3V3", "2": "GND", "3": "I2S_BCLK", "4": "I2S_WS", "5": "I2S_SD",
        "6": "UART_TX", "7": "UART_RX", "8": "EN", "9": "GPIO13", "10": "GPIO14"}),
]

# --- LED data level shifter: 3V3 LED_DATA -> 5V LED_DATA_5V ---
COMPONENTS.append(
    ("U8", "mixxtape_parts:SN74AHCT1G125DBVR", "SN74AHCT1G125",
     FP + "SOT-23-5_L3.0-W1.7-P0.95-LS2.8-BR", "C7484", False, (78, 56), {
         "1": "GND", "2": "LED_DATA", "3": "GND",
         "4": "LED_DATA_5V", "5": "VBUS"}))
COMPONENTS.append(
    ("C19", "mixxtape_parts:CL05B104KO5NNNC", "100nF", FP + "C0402", "C1525",
     False, (78, 61), {"1": "VBUS", "2": "GND"}))

# --- Reel LED chain: D1-D12 left ring, D13-D24 right ring, D25-D27 track,
# --- D28 REC, D29 BT.  WS2812B-2020 pinout: 1=DO 2=GND 3=DI 4=VDD.
# --- Fitted part is XINGLIGHT XL-2020RGBC-2812B (C5349955, WS2812B protocol,
# --- same land pattern) because the genuine Worldsemi part is EOL at JLC.
LED_FP = FP + "LED-SMD_4P-L2.0-W2.0-TL_WS2812B-2020"
for i in range(1, 30):
    col = (i - 1) % 5
    row = (i - 1) // 5
    din = "LED_DATA_5V" if i == 1 else f"LD{i - 1}"
    dout = "NC" if i == 29 else f"LD{i}"
    COMPONENTS.append(
        (f"D{i}", "mixxtape_parts:WS2812B-2020", "WS2812B-2020", LED_FP,
         "C5349955", False, (90 + 13 * col, 56 + 6 * row),
         {"1": dout, "2": "GND", "3": din, "4": "VBUS"}))
for k in range(8):
    COMPONENTS.append(
        (f"C{20 + k}", "mixxtape_parts:CL05B104KO5NNNC", "100nF", FP + "C0402",
         "C1525", False, (152, 56 + 3 * k), {"1": "VBUS", "2": "GND"}))

TEXTS = [
    ((12, 10), "USB-C + ESD + USB-UART"),
    ((90, 10), "POWER: USB-C only. Battery section DNP."),
    ((40, 56), "ESP32  (GPIO map: docs/cassette-recorder-agent-brief.md sec.5)"),
    ((8, 98), "I2S MEMS MIC (mono, L/R=GND)"),
    ((92, 84), "16MB AUDIO FLASH (VSPI)"),
    ((30, 102), "CONTROLS: 4 buttons + write-protect tab"),
    ((124, 82), "HACKER HEADER: 10 castellated pads"),
    ((88, 40), "TODO: CH340N has RTS# only, no DTR - BOM's S8050 auto-reset pair needs both signals."),
    ((88, 42), "  Fitted: cap-coupled RTS reset (C10) + hold-REC(IO0)-at-plug-in to enter bootloader."),
    ((88, 44), "  Alternative: CH340C/CH343 with DTR. Q1/Q2 omitted until decided."),
    ((88, 46), "DNP: TP4056+JST battery, IO34 battery-sense divider, IO35 line-in (jack needs Steven)."),
    ((62, 52), "U8: 3V3->5V LED data level shift"),
    ((88, 54), "REEL LEDS: D1-D12 left ring, D13-D24 right ring, D25-D27 track, D28 REC, D29 BT"),
]

# ---------------------------------------------------------------- emit

ROOT_UUID = u()


def esc(s):
    return s.replace("\\", "\\\\").replace('"', '\\"')


def snap(v):
    return round(v * G, 2)


def label_orientation(pin_angle):
    """Global label angle + justify so text extends away from the body."""
    return {
        0: (180, "right"),    # pin on left side, label extends left
        180: (0, "left"),     # pin on right side, label extends right
        270: (90, "left"),    # pin on top, label extends up
        90: (270, "right"),   # pin on bottom, label extends down
    }[pin_angle]


out = []
out.append(f'''(kicad_sch
\t(version 20250114)
\t(generator "gen_schematic.py")
\t(generator_version "9.0")
\t(uuid "{ROOT_UUID}")
\t(paper "A3")
\t(title_block
\t\t(title "Mixxtape - Cassette Recorder")
\t\t(date "2026-08-25")
\t\t(rev "0.1")
\t\t(company "funvill")
\t\t(comment 1 "Design spec: docs/cassette-recorder-agent-brief.md")
\t)''')

# lib_symbols: embed every used symbol
used_libs = sorted({c[1] for c in COMPONENTS})
out.append("\t(lib_symbols")
for lib_id in used_libs:
    block = LIBS[lib_id]
    short = lib_id.split(":", 1)[1]
    block = block.replace(f'(symbol "{short}"', f'(symbol "{lib_id}"', 1)
    out.append("\t\t" + block.replace("\n", "\n\t\t"))
out.append("\t)")

labels = []
noconnects = []
symbols = []

for ref, lib_id, value, footprint, lcsc, dnp, (gx, gy), nets in COMPONENTS:
    x0, y0 = snap(gx), snap(gy)
    pins = PINS[lib_id]
    sym_uuid = u()
    pin_lines = []
    for num in pins:
        pin_lines.append(f'\t\t(pin "{num}"\n\t\t\t(uuid "{u()}")\n\t\t)')
    props = []

    def prop(name, val, py_off, hide):
        h = "\n\t\t\t\t(hide yes)" if hide else ""
        props.append(
            f'\t\t(property "{name}" "{esc(val)}"\n'
            f'\t\t\t(at {x0} {round(y0 + py_off, 2)} 0)\n'
            f'\t\t\t(effects\n\t\t\t\t(font\n\t\t\t\t\t(size 1.27 1.27)\n\t\t\t\t){h}\n\t\t\t)\n\t\t)')

    ys = [p[1] for p in pins.values()]
    top = max(ys) if ys else 0
    prop("Reference", ref, -top - 5.08, False)
    prop("Value", value, -top - 2.54, False)
    prop("Footprint", footprint, 0, True)
    prop("Datasheet", "", 0, True)
    prop("Description", "", 0, True)
    if lcsc:
        prop("LCSC", lcsc, 2.54, True)

    symbols.append(
        f'\t(symbol\n'
        f'\t\t(lib_id "{lib_id}")\n'
        f'\t\t(at {x0} {y0} 0)\n'
        f'\t\t(unit 1)\n'
        f'\t\t(exclude_from_sim no)\n'
        f'\t\t(in_bom yes)\n'
        f'\t\t(on_board yes)\n'
        f'\t\t(dnp {"yes" if dnp else "no"})\n'
        f'\t\t(uuid "{sym_uuid}")\n'
        + "\n".join(props) + "\n"
        + "\n".join(pin_lines) + "\n"
        f'\t\t(instances\n'
        f'\t\t\t(project "mixxtape"\n'
        f'\t\t\t\t(path "/{ROOT_UUID}"\n'
        f'\t\t\t\t\t(reference "{ref}")\n'
        f'\t\t\t\t\t(unit 1)\n'
        f'\t\t\t\t)\n'
        f'\t\t\t)\n'
        f'\t\t)\n'
        f'\t)')

    for num, net in nets.items():
        if num not in pins:
            raise SystemExit(f"{ref}: pin {num} not in {lib_id}")
        px, py, pang = pins[num]
        ax, ay = round(x0 + px, 2), round(y0 - py, 2)
        if net == "NC":
            noconnects.append(f'\t(no_connect\n\t\t(at {ax} {ay})\n\t\t(uuid "{u()}")\n\t)')
        elif net:
            lang, just = label_orientation(pang)
            labels.append(
                f'\t(global_label "{net}"\n'
                f'\t\t(shape passive)\n'
                f'\t\t(at {ax} {ay} {lang})\n'
                f'\t\t(effects\n\t\t\t(font\n\t\t\t\t(size 1.27 1.27)\n\t\t\t)\n\t\t\t(justify {just})\n\t\t)\n'
                f'\t\t(uuid "{u()}")\n'
                f'\t\t(property "Intersheetrefs" "${{INTERSHEET_REFS}}"\n'
                f'\t\t\t(at {ax} {ay} 0)\n'
                f'\t\t\t(effects\n\t\t\t\t(font\n\t\t\t\t\t(size 1.27 1.27)\n\t\t\t\t)\n\t\t\t\t(hide yes)\n\t\t\t)\n'
                f'\t\t)\n'
                f'\t)')

texts = []
for (gx, gy), s in TEXTS:
    texts.append(
        f'\t(text "{esc(s)}"\n'
        f'\t\t(exclude_from_sim no)\n'
        f'\t\t(at {snap(gx)} {snap(gy)} 0)\n'
        f'\t\t(effects\n\t\t\t(font\n\t\t\t\t(size 1.7 1.7)\n\t\t\t\t(bold yes)\n\t\t\t)\n\t\t\t(justify left bottom)\n\t\t)\n'
        f'\t\t(uuid "{u()}")\n'
        f'\t)')

out.extend(noconnects)
out.extend(labels)
out.extend(texts)
out.extend(symbols)
out.append(f'\t(sheet_instances\n\t\t(path "/"\n\t\t\t(page "1")\n\t\t)\n\t)')
out.append('\t(embedded_fonts no)')
out.append(')')

OUT.write_text("\n".join(out), encoding="utf-8")
print(f"Wrote {OUT}")
print(f"  {len(COMPONENTS)} components, {len(labels)} labels, {len(noconnects)} no-connects")

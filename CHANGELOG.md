# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
This project is pre-release; entries are dated rather than versioned until the
first prototype run.

## [Unreleased]

### Changed (2026-08-30, NOR flash second-sourced and halved in price)

- **U3: Winbond W25Q128JVSIQ (C97521) → GigaDevice GD25Q128ESIG (C2758105)**,
  $2.2557 → $1.3348 at a run of 20. Net **$15.41 saved** after the one-off
  Extended-part setup fee the Winbond Basic part did not carry.
- The land patterns agree to within **0.02 mm**, so nothing in the layout
  moved. Command set (0x06, 0x05, 0x03, 0x02, 0x20, 0xD8, 0x9F) and the
  256 B page / 4 KiB sector / 64 KiB block geometry are identical, so the
  storage design is untouched.
- The **only** difference is the JEDEC manufacturer byte: 0xC84018 rather than
  0xEF4018. Rather than move the hard-coded constant, **both the driver
  (`spi_flash_hal.c`) and the factory test (`factory_main.c`) now accept
  either id** and log which part was found. C97521 therefore stays a real
  drop-in second source: if GigaDevice stock moves before the order goes in,
  a Winbond-populated board still boots with no firmware change. Do not
  narrow that check back to one value.
- Host tests: 11/11 pass.
- Board cost **$9.41 → $8.62**.

### Changed (2026-08-30, passives standardised on 0402)

- **The eight 10 k resistors moved R0603 → R0402** (C25804 → C25744). Cheaper
  as well as smaller — $0.0031 against $0.0053, Basic, 33 M stock. Every trace
  still reached its pad; DRC stayed clean with nothing to re-route.
- **C1 and C2 stay 0805.** The only Basic 10 µF in 0402 is 6.3 V, and C1 sits
  on the 5 V USB rail — 79 % of rating, where DC-bias derating would leave
  roughly 2 µF of the nominal 10, for 29 LEDs. Saving $0.07 a board is not
  worth three quarters of the bulk capacitance. Two passive sizes, not three.

### Fixed (2026-08-30, ERC was not actually clean)

- **Three dangling-wire errors** — 1.27 mm stubs at y=67.31 — left behind when
  the capacitive touch pads were removed. The removal script deleted the TP
  symbols and their global labels but not the wires between them.
- These had been reported as "ERC 0" because the report being read was a stale
  `mixxtape-erc.rpt` from 26 August. **`kicad-cli` writes its report relative
  to the working directory, not next to the board**, so running it from the
  repo root silently leaves the old file in `hardware/` untouched and
  believable. Both locations are now gitignored, and the checked-in copy is
  deleted — a generated report should never have been tracked.
- ERC is now genuinely 0 errors, 0 warnings.

### Fixed (2026-08-30, gen_layout.py would undo the board)

- The pre-production checklist told the reader to record case-fit measurements
  by editing `hardware/scripts/gen_layout.py` and re-running it. That script
  rebuilds the **original 100.33 mm outline** and would discard the trim to
  99.50 mm along with every placement and route since. The instruction now
  says to edit the board directly and re-run `export_fab.py`.

### Verified (2026-08-30, the board fits a real cassette case)

- Steven printed `case-fit-test-1to1.pdf` at 1:1 and checked it against a real
  plastic case: **it fits.** This retires the largest mechanical unknown in
  the project — the outline was borrowed from a board that fits *some* case,
  and the reel windows, tab position and mounting holes are ours and had never
  touched plastic.
- Still open: the same test against cases from **other brands**. One case
  proves the design is not wrong; it does not prove it is right, and internal
  ribs and hub-clamp ridges vary between manufacturers.

### Added (2026-08-30, fabrication package committed)

- `fab/` now holds the actual JLCPCB upload — gerbers zip, BOM and CPL — plus
  a README covering order settings and what to check in their previewer.
  Regenerate with `hardware/scripts/export_fab.py`, which refuses to run on a
  board that is not DRC clean and cross-checks the BOM against the placement
  file. Intermediate gerbers stay gitignored; the zip is the artefact.


### Changed (2026-08-30, bulk capacitors right-sized)

- **C1 and C2: 22 µF 25 V (C45783) → 10 µF 25 V (C15850)**, both Basic, same
  0805 footprint. Nothing on this board exceeds 5 V — C1 sits on USB VBUS and
  C2 on the regulated 3.3 V rail — so 25 V was 5× and 7.5× margin respectively.
- **JLCPCB's Basic library has no 10 V or 16 V part in these sizes**, only 25 V,
  so dropping the voltage rating would have meant leaving Basic and paying a
  setup fee. Dropping the *capacitance* instead keeps it Basic and costs less.
- Counter-intuitively this loses almost no real capacitance. Ceramics derate
  under DC bias: a 22 µF 6.3 V part at 3.3 V delivers perhaps 8–10 µF, while a
  25 V part barely derates. **A 10 µF 25 V cap gives more capacitance in
  circuit than a 22 µF 6.3 V one**, at a fraction of the price.
- **C9: 1 µF Extended (C29266) → Basic (C52923)**, 25 V rather than 16 V. The
  other five Extended parts — LED, USB-C, ESP32, microphone, level shifter —
  have no Basic equivalent at all.
- Board cost **$10.01 → $9.41**, setup fees $18 → $15.

### Fixed (2026-08-30, part numbers scrambled twice by careless patching)

- Two attempts at these swaps split the schematic on `(symbol "` and applied a
  replacement to every resulting chunk. The final chunk runs to end-of-file and
  therefore contains **every component instance**, so one symbol's part number
  was written onto all 66 parts. Both times the generated BOM caught it
  immediately — every line collapsed onto a single part and the cost fell to
  something absurd.
- Now fixed properly: the `lib_symbols` block is located by matching
  parentheses, and library edits are confined to it by construction, with an
  assertion that no instance can appear inside.
- The repair also surfaced a **second, redundant `LCSC Part` property** that
  easyeda2kicad writes alongside `LCSC`. Nothing had ever maintained it, so it
  was stale on several symbols — including WS2812B-2020, which was carrying the
  Worldsemi part number the project stopped using months ago. Both properties
  now agree on every symbol.
- Verified: ERC 0 · DRC 0 · board and schematic agree on all 287 pads · every
  instance's part number checked against the expected design, count by count.

### Changed (2026-08-29, board narrowed to 99.50 mm)

- **The outline is now 99.50 × 63.50 mm**, trimmed 0.83 mm off the left edge
  where there are no components. The left edge and both left corners moved
  right; everything else — copper, silkscreen, every part — stayed exactly
  where it was.
- **The two left mounting holes moved with the edge.** Left in place they would
  have finished 0.82 mm from the new edge instead of 1.65 mm, which is not
  enough rim for a mounting hole.
- **A bug in the first attempt was caught by measuring, not by DRC.** The shift
  was applied per coordinate, so for the two left mounting holes the centre
  moved but the radius-defining end point did not — turning them from Ø2.20
  into Ø0.54. A 0.54 mm hole is perfectly legal, so DRC passed it without
  comment. Circles are now moved as whole objects.
- **`gen_silk_template.py` now reads the board size from Edge.Cuts** instead of
  hardcoding it, so the artwork template cannot drift from the outline again.
  `gen_layout.py` carries a warning: re-running it would rebuild the original
  100.33 mm outline.
- Propagated to the README, the pictogram sheet, the fabrication README, the
  B-side designer brief — including its dimensioned keepout table and its scale
  drawing — and the bounds constants in `add_zones.py` and `gen_placement.py`.
- Verified: DRC 0 errors, 0 unconnected; outline closed with no dangling
  endpoints; every other cutout unchanged in position and diameter.

**Note for the artwork:** the peony design was drawn full-bleed to 100.33 mm.
It still covers the board, but 0.83 mm now falls off the left edge and the
composition sits fractionally off-centre. Worth a look before the run.

### Added (2026-08-29, JLCPCB fabrication export)

- **`hardware/scripts/export_fab.py`** builds the whole package: gerbers, drill
  files, the placement file remapped to JLCPCB's column names, the BOM, and the
  zip. Settings follow JLCPCB's own KiCad guide — Protel extensions, soldermask
  subtracted from silkscreen, zone fills checked, Excellon in millimetres with
  a decimal zeros format and absolute origin. X2 is off for the widest CAM
  compatibility.
- **It refuses to export a board that is not DRC clean**, and it cross-checks
  the BOM against the placement file: a designator in one and not the other
  stops the run. Both files agree on all 66 designators.
- Drill map gerbers are generated but deliberately kept out of the zip — they
  are documentation, and a stray `.gbr` can be mistaken for a copper layer by
  an auto-detector.

### Fixed (2026-08-29, two parts would have been assembled wrongly)

- **DNP is stored per footprint on the board, not only in the schematic.** J4
  and R9 were marked Do Not Populate in the schematic but carried no DNP
  attribute on the board, so `--exclude-dnp` had nothing to act on and both
  landed in the placement file. An order placed from that export would have had
  **a microSD socket fitted to every board**.
- **J5, the programming jig pads, was in the placement file too.** Its
  footprint had `exclude_from_bom` but not `exclude_from_pos_files` — an
  omission from when it was first drawn. It is bare copper, not a part.

### Changed (2026-08-29, R9 is fitted)

- **R9 is now populated**; only J4 stays DNP. R9 is the pull-up that keeps the
  SD chip-select pin defined with the socket unfitted. Its value drops the
  "(DNP)" suffix and it folds into the 10 k line, which becomes ×8.

### Removed (2026-08-29, capacitive touch pads)

- **The touch pads are out.** TP2/TP3/TP4, their footprints, all 29 track and
  via segments on the TOUCH nets, the symbols, the labels, and the pin block in
  `board.h`. GPIO 2, 13, 14 and 15 are unallocated again, with their no-connect
  markers restored so ERC knows they are unused on purpose.
- **The footprint and symbol stay in the libraries.**
  `mixxtape_local:TOUCH_PAD_12` and its schematic symbol cost nothing sitting
  there, and they carry the working geometry — a 12 mm pad under the solder
  mask with a no-pour rule area on both layers — if the idea returns.
- The line-out document's rejection reasoning was corrected: it had said its
  pins were taken by touch pads, which is no longer true. It is still rejected,
  on scope rather than feasibility, and IO26/IO13/IO14 are free again.
- Verified after removal: **ERC 0, DRC 0, board and schematic agree on all 287
  pads**, and no `TOUCH` reference survives in the board, the schematic or the
  firmware.

### Added (2026-08-29, capacitive touch pads on the back)

- **Four Ø12 mm touch pads on B.Cu**, TP1-TP4, on the ESP32's touch channels
  **T2/T3/T4/T6 (GPIO 2, 15, 13, 14)**. Copper under the solder mask — mask is
  a good dielectric for sensing and it keeps the copper from tarnishing, so
  nothing is exposed. **No BOM cost at all**: a touch pad is a shape in the
  copper.
- **T5 (GPIO12) was available and was deliberately skipped.** It is the MTDI
  strap that selects flash voltage, and a floating copper plane is the last
  thing to hang on it — read high at boot and the module does not start. T2 and
  T3 are also strapping pins but only for boot mode and log suppression, and a
  pad adds capacitance rather than a DC level.
- The footprint **carries its own no-pour rule area on both layers**, so the
  ring that isolates the pad from the ground pour, and the cut-out that keeps a
  plane from sitting directly behind the sensor, travel with the pad wherever
  it is dragged.
- **Left unrouted on purpose.** The pads are meant to be moved to suit the
  artwork, and routing now would only have to be undone.
- Pin definitions added to `board.h`, including the note that touch v1 on the
  original ESP32 drifts with temperature and humidity, so the firmware wants a
  slow baseline rather than a fixed threshold.

### Removed (2026-08-29, line-out option closed)

- **The 3.5 mm jack is not being built.** `docs/line-out-option.md` is marked
  rejected rather than deleted — the part numbers, stock figures and costs in it
  were all verified and stay useful, and it records why IO13 and IO14 went to
  touch pads instead.

### Fixed (2026-08-29, placement caught by DRC)

- The first touch-pad placement dropped **TP3 straight on top of the button
  signals and their vias** on the back copper — ten shorts and four hole
  clearance errors. The space scan behind it only checked footprint courtyards,
  not tracks and vias. Rescanned properly against the back copper, which turned
  out to be far denser than the footprint map suggested, and re-placed.

### Added (2026-08-29, wordless quick-start pictograms)

- **`docs/quick-start-pictograms.html`** — IKEA-style instructions for the four
  things people actually do: pair, play, record, next track. No prose in the
  panels; a printable single sheet.
- **The device is drawn to its real proportions** — 100.33 × 63.50 mm, with the
  reel windows, the five status lights, the four buttons, the write-protect
  tongue, the microphone and the USB-C port all in their true positions. The
  picture matches the object in your hand rather than a generic cassette.
- Gestures were taken from `ui_state.c` rather than from the prose, which
  turned up that **MODE has three hold tiers** — 2 s pair, 5 s dub, 10 s
  dev-wifi — and that under 2 s it deliberately does nothing.
- Three things a picture cannot carry are stated in words at the foot:
  holding PLAY for a second sleeps the device, recording overwrites the
  selected track from the beginning, and MODE ignores short presses on purpose.

### Added (2026-08-29, line-out option assessed)

- **`docs/line-out-option.md`** — the write-up for adding a 3.5 mm jack, for
  line-out to a powered amp or wired headphones. Proposed only; nothing is on
  the board.
- It comes out cheap because four things happen to already be true: IO26, IO13
  and IO14 are free and unstrapped (IO26 was freed by the microphone's move to
  PDM); the mic sits on `I2S_NUM_0` so **I2S1 has never been touched**;
  playback is already pull-shaped, so an I²S output task calls the same
  `tape_player_read` the A2DP path does; and `bt_audio.c` already duplicates
  mono into interleaved stereo, which is exactly what a stereo DAC needs.
- **~$1/board and about two days of work**, most of it layout — the board is
  fully routed at zero DRC errors, so three new signals crossing 40 mm means
  re-routing, re-pouring and re-checking the placed B-side artwork.
- **Surface-mount jacks are plentiful.** PJ-320D ([C431535](https://jlcpcb.com/partdetail/C431535))
  has 79,274 in stock at $0.037. Confirmed surface-mount by reading footprint
  pad data rather than trusting JLCPCB's category field, which is unreliable
  for connectors.
- **Records the trap:** the MAX98357A is the usual ESP32 audio answer and is
  wrong for a jack — its bridged output has no ground reference, so a plug's
  sleeve shorts half the bridge. It is for a soldered speaker.
- The document separates what was verified from what was not, including the
  claim that a PCM5102A cannot drive 32 Ω headphones — that follows from its
  datasheet role, not from measurement.

### Changed (2026-08-29, documentation consolidated)

- **The README's hardware summary now carries JLCPCB part numbers, each linked
  to the part page**, plus per-part quantities. Rewriting it turned up three
  errors that had been sitting there: the microphone was still listed as
  MSM261S4030H0R, as I²S, and as bottom-ported. All three were corrected
  elsewhere months ago and never here. The cost was also stale at $13.25/board
  against the generated BOM's $10.01.
- **The agent brief was the worst of the drift.** It still specified the CH340N
  USB-UART and USBLC6 ESD diode (both deleted when USB-C went power-only), SBC
  encoding at record time (now ADPCM), a castellated header and a lanyard hole
  (neither exists), UART routed to the CH340N, and the superseded BOM as
  authoritative. All corrected.
- **`cassette-recorder-bom-v3.md` deleted.** Its one lasting value — the list of
  seven ways a hand-maintained BOM drifted from the board — now sits at the top
  of the generated BOM, where someone tempted to hand-edit will see it.
- **Added `docs/README.md`**, an index grouping the documents by what they are
  for, and naming which files are generated and by which script. Every relative
  link across the repository's markdown now resolves.
- The pre-production checklist gained a manufacturability section from the
  JLCPCB audit, and lost its references to the lanyard hole, which the board no
  longer has.

### Checked (2026-08-29, USB-C receptacle)

- **No board notch is needed.** The TYPE-C-31-M-12 is a top-mount right-angle
  receptacle; its only board penetrations are four plated shell legs and two
  non-plated locating pegs, all already in the footprint. Notches are for
  *mid-mount* receptacles, which sit partly through the board. This is not one.
- Its mouth sits **0.76 mm inboard of the board edge**, with the strip in front
  clear of copper and components, and the edge there is a straight run from
  y 2.54 to 60.96. The plug's shell passes over that lip because the mouth is
  raised above the board surface.
- Open question for the checklist: a closed Norelco case has a wall on that
  edge, so decide whether the board comes out to charge or the case is modified.

### Fixed (2026-08-29, manufacturability audit against JLCPCB)

- **The microphone was tagged `through_hole` while carrying eight SMD pads.**
  A footprint's attribute decides whether JLCPCB's pick-and-place file lists
  the part, so U2 could have been dropped from the CPL and left unassembled on
  every board. J1 had the mirror of the same fault, tagged `smd` with four real
  plated through-hole pads. Both corrected in the board and the libraries.
- **Eleven DRC rules were set to `ignore`**, hiding 145 findings — every
  silkscreen check, every courtyard check, footprint type mismatch and mirrored
  text among them. The mic's wrong attribute is exactly what
  `footprint_type_mismatch` exists to catch, and it had been switched off. The
  eight that matter are back on as warnings.
- **80 silkscreen strokes were below JLCPCB's 0.15 mm minimum**, some as thin as
  0.06 mm, inherited from the easyeda2kicad footprints; the board default was
  0.10 mm. Raised to 0.15 mm in the board and the libraries. Filled artwork
  polygons were left alone — their stroke is cosmetic, the fill is what prints.

### Checked (2026-08-29, against JLCPCB 2-layer capabilities)

- Narrowest track 0.150 mm against a 0.10 mm limit; vias 0.60/0.30 mm against
  0.25/0.15 mm; copper-to-edge held at 0.40 mm against 0.20 mm. All comfortable.
- **Two figures sit close to the line.** The smallest plated annular ring is
  0.200 mm on J1, under JLCPCB's recommended 0.25 mm though above the 0.18 mm
  floor. Closest hole-to-hole is 0.500 mm against a 0.45 mm limit, inside the
  DNP microSD footprint.
- **White solder mask is confirmed available**, adds about two days, and pairs
  with **black silkscreen as a fixed choice** — that settles an open question in
  the B-side brief.
- **The B-side artwork carries 32 shapes thinner than 0.15 mm** in one dimension
  and 52 more between 0.15 and 0.25 mm. The first group will not survive the
  screen. Recorded in the brief for the designer.

### Added (2026-08-29, README artwork)

- The 3D turntable render and the case artwork mockup now open the README, with
  a note that the render's green mask is the viewer's default rather than the
  matte white the board ships in.
- `artwork/exec-31918c24-…png` renamed to `artwork/case-mockup.png`.

### Added (2026-08-28, cassette insert template)

- **Double-sided J-card template for the case insert**, 1:1, as two SVGs and a
  two-page PDF. Panels follow the stepped standard — front 65.0875 mm, spine
  12.7 mm, tracklist flap 26.9875 mm, card 101.6 mm across — where each folded
  panel is 1/16 inch narrower than the one before so the card nests inside its
  own folds instead of binding.
- The 101.6 mm runs horizontally, matching the board's 100.33 mm, so the front
  panel reads upright at 101.6 x 65.09 and the folds are horizontal.
- **Two pages, card in the same position on each**, because that is what duplex
  printing needs. Long-edge flip; the card is centred, so every panel backs
  itself.
- The tracklist is **ruled and deliberately blank**. The owner records the
  tracks, so printing a fixed listing would be wrong — it is a write-on field,
  the same reasoning as the matte white mask on the board.
- Carries the same 50 mm calibration bar as the case-fit sheet, plus cut, fold,
  safe-area and bleed marks.

### Fixed (2026-08-27, CI: the `bt` component name collided with ESP-IDF's)

- **`firmware/components/bt` is renamed to `components/btaudio`.** ESP-IDF
  ships its own component called `bt`, and project components override
  IDF's by directory name, so ours silently shadowed the real Bluetooth
  component. `esp_hid` then lost the include paths it inherits through
  `bt` and the build died on `esp_timer.h: No such file or directory` —
  an error inside IDF's own source, which is why it read as an IDF problem
  rather than ours.
- **The IDF build has been red since 3dcf692** ("pairing policy and
  persistent bond table"), the commit that created the component. Four
  consecutive failures, all the same cause. Host tests were green
  throughout, which is why it went unnoticed.
- The constraint is now written down in `firmware/README.md`, because the
  next person to add a component called `console`, `driver` or `log` will
  hit exactly this.

### Changed (2026-08-27, CI runs on pull requests only)

- The firmware workflow no longer runs on push; it runs on `pull_request`
  and `workflow_dispatch`. **This repo commits straight to main, and a push
  to main opens no pull request** — so unless the habit changes to raising
  PRs, CI now only runs when triggered by hand from the Actions tab.
- `actions/checkout` bumped v4 → v5, clearing the Node 20 deprecation
  warning that was annotating every run.

### Removed (2026-08-27, battery section)

- **The TP4056 charger, its JST battery connector and PROG resistor are
  gone** (U5, J2, R8, plus the VBAT and PROG nets). They were always DNP,
  and they could never have worked: VBAT dead-ended at the connector and
  the charger, and even wired through, a lithium cell at 3.0–4.2 V cannot
  feed an AMS1117 that needs 1.1–1.3 V of headroom to make 3.3 V. The LEDs
  and the level shifter sit on the 5 V USB rail besides. Running from a
  cell means a different regulator and a different LED rail — a redesign,
  not a populated footprint. Removing it frees about 40 mm² on a board
  that is about to be routed.
- Docs no longer promise a battery footprint you can solder to. "USB-C
  only" was already the locked decision; now the board matches it.

### Fixed (2026-08-27, schematic review before routing)

- **The board outline was self-intersecting and would have failed CAM.**
  The write-protect tab's two slots were drawn as closed rectangles lying
  *on top of* the top edge instead of cut into it, so the edge ran straight
  through both — four real crossings. Split the top edge into three spans
  and removed the slot mouths. The outline is now a single closed loop with
  no crossings and no dangling endpoints, and the stale zone fills that
  came with it are refilled.
- **Every symbol pin now has a real electrical type.** easyeda2kicad types
  every pin `unspecified`, which silently disables the checks that matter.
  109 pins retyped from the datasheets, in the schematic *and* in
  `mixxtape_parts.kicad_sym` so a library update cannot undo it. Turning
  the checks on immediately found three genuine faults: U4's two VOUT pins
  and J1's paired VBUS and GND pins each read as two power outputs fighting
  over one rail. They are duplicate pins of one physical node, so one of
  each pair now carries the `power_out` and its twin is `passive`.
  **ERC: 191 warnings → 0 violations.**
- **TAB1 had no footprint at all**, so TAB_SENSE would have read
  permanently low. Added `mixxtape_local:TAB_BREAKOFF_LINK`, a two-pad net
  tie placed on the break-off tongue: snap the tongue and the link goes
  with it. Note for routing — both nets must cross the perforation through
  the gaps between the bite holes, and freerouting does not understand net
  ties, so hand-route those two first.
- **C9 was 100 nF on EN; the ESP32-WROOM-32E datasheet asks for 1 µF**
  (§ "it is advised to add an RC delay circuit at the EN pin… R = 10 kΩ and
  C = 1 µF"). Changed to C29266.
- **Added R12, 100 Ω (C25076), in the microphone's supply**, per figure 1
  of the MSM261DHT006 datasheet. C7 moves onto the new MIC_VDD net so the
  two form an RC filter — left on 3V3 it would just be another rail cap.
  A PWR_FLAG marks MIC_VDD as driven, and `gen_bom.py` now skips
  `in_bom no` symbols so the flag is not a phantom BOM line.
- **Four through-hole pads had zero annular ring** (J1 ×2, J4 ×2) — the
  connectors' moulded locating pegs, where pad size equalled drill size.
  Now `np_thru_hole`, fixed in the board and in the library footprints.
- **Datasheet links corrected.** D1–D29 pointed at Worldsemi C965555 while
  the fitted part is XINGLIGHT C5349955; the two symbols cloned for R12 and
  C9 had inherited their donors' links.

### Added (2026-08-27, mounting holes)

- **Three Ø2.2 mm mounting holes** at the top-left (32.75, 32.75),
  bottom-left (32.75, 90.75) and top-right (122.50, 32.75), each with the
  Ø4.2 mm annotation ring the existing holes use. The top-right one sits
  8.3 mm inboard rather than in the corner because the microphone is there
  — there is no room between it and the board edge. Moving the mic ~3 mm
  left would free (127.58, 32.75) for a symmetric pattern.
- **`mixxtape.kicad_dru`**, scoping a 0.1 mm clearance exception to J1. The
  USB-C receptacle's own pads sit 0.10 mm apart against the board's 0.20 mm
  rule; that is the connector's geometry, not our routing.
- Verified: **ERC 0, DRC clean apart from the 130 unrouted nets, every
  footprint's pads reconcile with its symbol's pins, and every board pad
  agrees with the schematic netlist.**

### Changed (2026-08-25, microphone: MSM261DHT006, PDM)

- **Microphone changed to MSM261DHT006 (C51928215)** across schematic,
  layout, firmware and docs, replacing MSM261S4030H0R (C2840615) which is
  at zero stock. Acoustically an equal swap — identical −26 dBFS
  sensitivity, same 1.6–3.6 V range, same 4×3 mm LGA-8 outline, 1 dB less
  SNR and 2 dB *better* acoustic overload — at $0.45 against $1.61.
- **This was a schematic change, not a part-number swap.** The pinout is
  completely different: pin 1 is VDD where the old part had GND, L/R moves
  to pin 2, and pins 5–8 are all GND. Footprint changed to
  `LGA-8_L4.0-W3.0-R_WMM7040DT0`.
- **Interface moves from I²S to PDM.** Two wires instead of three, so
  **GPIO26 is freed**. GPIO25 becomes the PDM clock, GPIO33 the data input.
  The ESP32's I²S peripheral does PDM receive and decimation in hardware,
  so nothing downstream of the capture task changes.
- **PDM downsample ratio must be 64.** PDM clock = sample rate × ratio, and
  44100 × 128 = 5.64 MHz exceeds the microphone's 4.8 MHz maximum. 64 gives
  2.8224 MHz, inside its window. Recorded in `board.h` because it is the
  obvious mistake to make.
- Board cost drops to **$9.85/board** at 20 boards, from $11.01.
- ERC 0 errors, DRC 0 violations, 11 host suites still green.

### Fixed (2026-08-25, microphone datasheet review)

- **The brief's "bottom-ported" microphone is actually top-ported.** The
  specified MSM261S4030H0R datasheet says so on page 2 ("omnidirectional,
  Top-ported, I²S digital output"), and so does every in-stock alternative
  in the family. The 0.5–0.8 mm acoustic hole the brief called for through
  the board would have done nothing except weaken it — sound enters from
  the component side. Hole removed from the mechanical layout and replaced
  with a keep-clear annotation; the brief is corrected at source.

### Findings (2026-08-25, microphone replacement)

- **MSM261DHT006 (C51928215) is acoustically an equal swap**, not a
  downgrade: identical −26 dBFS sensitivity, same 1.6–3.6 V range, same
  4×3 mm LGA-8 footprint, 1 dB less SNR (60 vs 61 dB) and 2 dB *better*
  acoustic overload point (122 vs 120 dB SPL) — which matters for a
  recorder that will meet the occasional door slam. Cheaper too ($0.45 vs
  $1.61) and lower current.
- **Its one real cost is the interface: PDM, not I²S.** Firmware would
  initialise the ESP32's I²S peripheral in PDM RX mode, which the original
  ESP32 supports in hardware. This is a firmware change, not a hardware
  one — the pin count and footprint are unchanged.
- **Alternative if the firmware change is unwelcome:** ICS-43434
  (C5656610, ~3.7k, $3.33) keeps I²S and gains 5 dB of SNR (65 dB) on the
  project's riskiest subsystem — but costs 7× more (+$58 across 20 boards)
  and has a different 3.5×2.65 mm footprint, so symbol and land pattern
  both change.

### Added (2026-08-25, supply-chain rule and the case-fit test)

- **Supply-chain rule (Steven):** every fitted part must have 5,000+ in
  stock at JLCPCB, and a common part beats a capable one. Enforced by
  `gen_bom.py`, which now prints a rule section; `find_common_parts.py`
  searches for compliant alternatives. Exceptions must be argued in
  `STOCK_EXCEPTIONS` rather than waved through.
- **Audit result: every fitted part clears 5,000 except the microphone.**
  The nearest thing to a problem elsewhere is the ESP32 module at 24k,
  which is a locked decision anyway (only ESP32 with Classic Bluetooth).
- **⚠ No digital MEMS microphone at JLCPCB meets the rule.** The whole
  category is thin — the best-stocked digital part is ~3.5k. The only mic
  that clears 5,000 (ZTS6216, C481302, 29k) is **analog**: it needs a
  DC-blocking cap and a gain stage into the ESP32's ADC, which the brief
  already calls a lo-fi path with ~9 effective bits. Meeting the rule there
  would wreck the product's only input, so a digital part at ~3.5k is
  recorded as an accepted exception with three qualified alternates.
- `docs/case-fit-test-1to1.pdf` — printable board outline at 1:1, with a
  50 mm calibration bar so a silently scaled print is caught before
  anything is measured against it.
- `docs/PRE-PRODUCTION-CHECKLIST.md` — the case-fit procedure, every open
  decision, what to re-quote, and the three prototype questions that could
  still change the hardware. Linked from the README.

### Added (2026-08-25, PCB mechanical layer)

- `hardware/mixxtape.kicad_pcb` — board outline, reel windows, break-off
  write-protect tab, lanyard hole and microphone port. Generated by
  `hardware/scripts/gen_layout.py`; DRC clean. **Mechanical only** — no
  components placed and no routing, which is the brief's phase 2 and the
  part that has to be checked against a real case before any copper.
- **The outline is not redrawn from a standard.** Per the brief, it is
  traced from Open Music Labs' Mixtape Alpha released gerbers
  (`hardware/scripts/cassette_outline.json`, 104 segments,
  100.33 × 63.50 mm) — a cassette shape that has actually been fabricated
  and fits real cases, which beats a dimension copied out of a datasheet.
- Reel hubs at the compact-cassette standard 42.0 mm spacing, 34.3 mm from
  the bottom edge (measured off the Mixtape Alpha board render — they sit
  slightly above centre, which is what makes it read as a cassette). Ø14 mm
  windows with the twelve LED positions marked on a Ø18 mm ring.
- Tab is 9 × 5.5 mm at the top-left, held by five 0.6 mm mouse bites, with
  routed slots either side.

### Needs Steven (2026-08-25)

- **⚠ Licence of the borrowed outline is unstated.** The Mixtape Alpha wiki
  publishes the board files but names no licence for them (the "GPL
  licensed" on that page refers to the wiki software). Confirm with Open
  Music Labs before distributing derived artwork. Mitigation if they
  decline: the shape is the compact-cassette form factor, so redrawing it
  from a measured cassette is straightforward.
- **Print the outline at 1:1 and put it in a real case.** Reel window
  diameter versus hub-clamp ridges, tab position versus internal ribs, and
  the lanyard hole versus the hinge are all still assumptions. Case brands
  are not dimensionally consistent — the brief says buy several.
- The five decorative guide holes along the bottom edge are deferred: they
  never enter a deck, so they are artwork rather than mechanism, and
  guessed positions would constrain component placement for nothing. Add
  them with the silkscreen artwork.

### Added (2026-08-25, BOM v4 — generated, with live pricing)

- `hardware/scripts/gen_bom.py` — derives the BOM from
  `hardware/mixxtape.kicad_sch` and reads prices, stock and library type
  live from JLCPCB, emitting both `docs/cassette-recorder-bom-v4.md` and a
  JLCPCB-format assembly CSV. Requests are paced and cached because JLC
  throttles bursts.
- **BOM v3 had drifted from the schematic in seven places** and is now
  marked superseded: it still listed the CH340N, USBLC6 and S8050 pair
  (deleted when USB-C became power-only) and the ME6211 LDO (replaced by
  the AMS1117), and was missing the fitted WS2812 substitute, the
  SN74AHCT1G125 level shifter and the DNP microSD footprint. Generating it
  removes that whole class of error.

### Findings from live JLCPCB data (2026-08-25)

- **⚠ The microphone is out of stock.** MSM261S4030H0R (C2840615) shows
  zero at JLC — and it is the one part the product cannot ship without.
  Same-family alternatives are in stock at roughly a third of the price
  (C51928215, C22390138, C51928210), but the part-number suffix encodes
  package and port direction, and this design needs a **bottom-ported**
  mic because there is an acoustic hole through the board beneath it. Each
  candidate needs its datasheet checked before substituting. Needs Steven.
- NOR flash is up as the brief predicted: W25Q128 is $2.26 against v3's
  $1.65.
- Parts now cost **$11.01/board** at 20 boards ($205.20 components +
  $15.00 in setup fees for 5 extended parts), against v3's ~$8.50 estimate
  for components. PCB and assembly labour are quoted separately.
- At this quantity the per-extended-part setup fee dominates: adding a new
  extended part costs $3, while using more of one already on the board is
  nearly free.

### Added (2026-08-25, user documentation)

- `docs/MANUAL.md` — the manual as it would ship with a board: getting
  started, the controls, recording and playback, the light language, the
  write-protect tab and why it is permanent, power, labelling, care,
  troubleshooting, specifications, and a plain list of what the device
  deliberately does not do.
- `docs/FAQ.md` — the questions people actually ask, answered against the
  design as built, with a closing section of honest limitations (no way to
  get recordings onto a computer, twelve minutes total, mono, handling
  noise still unproven, needs mains power, will not play in a deck).
- Both carry a draft banner and mark unverified claims as such — sound
  quality, handling noise, AirPods compatibility and reconnect latency are
  design intent until prototypes exist, and the docs say so rather than
  implying they are settled.
- Neither document uses the project's current name, since it collides with
  Mixxim's commercial product and a rename is planned; they say "your
  tape" throughout, which reads better anyway and makes the rename a
  non-event.

### Added (2026-08-25, firmware — record-path limiter)

- `components/audio/limiter` — look-ahead peak limiter with bounded makeup
  gain, replacing the whole-take peak normalisation that one-pass recording
  gave up. One gain does both jobs:
  `target = min(max_gain, ceiling / envelope)` — loud rooms are pulled down
  and cannot clip, quiet rooms are lifted (up to +18 dB) so an ambient take
  is actually audible. It is also the better tool than normalisation, which
  a single door slam defeats.
- The envelope is a true **sliding maximum** over a window that includes
  the sample about to leave the delay line, not a decaying peak follower.
  That makes `|output| <= LIMITER_CEILING` a hard guarantee for any input
  rather than a fast attack that usually keeps up — a decaying signal
  defeats the cheap version, and there is a test for exactly that case.
- Gain falls immediately and rises slowly: the immediate fall lands on
  audio still inside the 10 ms delay line, so it is inaudible, while the
  slow rise (~371 ms) stops the noise floor pumping between words. A gate
  freezes the gain below -44 dBFS so a silent room is not amplified into
  hiss. Integer throughout, so host and target results are identical.
- Also feeds the REC lamp and reel brightness via `limiter_level()`.
- Host tests: the ceiling invariant is checked against full-scale squares,
  near-Nyquist content, rail-to-rail alternation, noise and a decaying
  tone; look-ahead is proven by a silence-to-full-scale burst whose *first*
  cycle already sits at the ceiling; plus delay accounting, gain bounds,
  gate behaviour, smooth recovery, in-place aliasing, and invariance to
  chunk size. 11 suites, ~62,000 assertions, green in ~1.8 s.

### Fixed (2026-08-25)

- Level meter decay stalled at 3/255 because the shift-based decay
  underflows below 2^9, which would have left the REC lamp faintly lit
  forever after a loud take. Caught by `test_limiter`.

### Added (2026-08-25, firmware — pairing policy and bond storage)

- `components/bt/pairing` — the selection policy that makes "hold the board
  against the speaker" the entire interaction. Class-of-Device filtering
  drops phones, laptops, peripherals and wearables so the user never sees a
  list of their own hardware, and rejects the non-sink members of the
  Audio/Video class (microphones, set-top boxes, VCRs, cameras, monitors).
  RSSI then makes proximity the selection mechanism. Ties keep the earlier
  entry so repeated inquiries do not make the board look like it is
  dithering. Speakers that omit the Audio service bit are still accepted —
  plenty of cheap ones do.
- Reconnect backoff with the "sink busy" signal: the sink is usually busy
  reconnecting to its owner's phone at power-up, so attempts back off
  exponentially to a cap, and after ~15 s the user is told honestly rather
  than left watching a hopeful blink. Correct across the millisecond
  counter wrapping (~49 days).
- `components/bt/bonds` — persistent bond table in the spare flash region,
  reusing the tape header's append-only ping-pong log: 8 bonds, ordered
  most-recently-used first (which is the order power-up tries them),
  least-recently-used eviction when full, and crash-safe commits.
- Host tests: a power-yank sweep over the bond log proves an established
  bond survives a cable pull during a new pairing, the new bond either
  lands completely or not at all, and the table is still usable
  afterwards. 10 suites, ~60,000 assertions, green in ~1.9 s.

### Added (2026-08-25, firmware M5 — factory test harness)

- `components/factory` — the harness that gets 20 hand-built boards
  checked identically: sequences probes, aggregates results, and emits an
  operator table plus a one-line machine record
  (`FT|1|flash_id=PASS:0xEF4018|...|VERDICT=PASS`) so the jig's serial log
  becomes the build record for the run.
- Design properties, all host-tested with mock probes: every step runs even
  after one fails (a board with three faults reports three, not one per
  reflash); each result carries a measured value, not just a verdict;
  skips never fail a board; interactive steps are skipped automatically on
  unattended runs; both report formats truncate safely at every buffer size
  (swept 1..200 bytes).
- `main/factory_main.c` — board-side probes and the runner, built with
  `idf.py -DMIXXTAPE_FACTORY_TEST=ON`. Tab sense, button walk and the SPI
  JEDEC ID read are written; mic, LED walk, CC sense, flash read/write and
  BT inquiry are stubs reporting SKIP until the M4 drivers exist. The
  runner prints `SUITE INCOMPLETE` rather than a pass banner while any
  probe is unimplemented — a green light nobody earned is worse than none.
- 8 host suites, ~59,000 assertions, green in ~1.6 s.

### Added (2026-08-25, firmware — playback and the reel display)

- `components/tape/tape_player` — the tape-side sequencer. PLAY runs every
  recorded track in order as one side, skipping empty slots rather than
  playing silence; pause/resume, track skip with wrap, and position
  measured across the whole side (which is what the reels show). Pull-based
  so the A2DP callback, which must never block, only ever copies out of a
  ring a prefetch task fills.
- `components/leds/reels` — the reel display as a pure function of (mode,
  progress, level, clock). Left ring empties while the right fills; red
  pulse riding the input level while recording; amber sweep hunting for a
  sink; green lock; dim reverse spin during background erase; violet while
  dubbing; slow red pulse when the sink is busy.
- Host tests: `test_tape_player` (side sequencing verified sample-exact
  against independently decoded audio, empty-slot skipping, pause/resume,
  skip wrap, monotonic position, playback after remount) and `test_reels`.
  7 suites, ~58,000 assertions, green in ~1.3 s.

### Verified (2026-08-25)

- **LED power budget holds.** `test_reels` sweeps every mode across time at
  worst-case inputs and measures the frame: **119 mA worst case** at the
  default 500 mA-source brightness cap, 227 mA at the 1.5 A cap — against a
  200 mA slice of the USB budget (the ESP32 alone bursts to ~240 mA on BT
  transmit). No mode ever renders all-white. The constraint is now enforced
  by a test rather than by hoping.
- Reel conservation: the two rings together always hold the same amount of
  "tape" across a whole side, to within integer rounding.

### Changed (2026-08-25, firmware M3 — stored format is ADPCM, not SBC)

- **ESP-IDF's A2DP source accepts PCM only.** `esp_a2d_source_data_cb_t` is
  handed a "buffer to be filled with PCM data stream"; Bluedroid encodes SBC
  internally and exposes no API for pre-encoded frames. The brief's
  "playback is read-frames-and-transmit" is therefore unbuildable as
  written — storing SBC would mean decoding it back to PCM at playback just
  so the stack could re-encode it.
- **Tape now stores block-framed IMA ADPCM.** 512-byte blocks: 4-byte header
  (predictor, step index, payload checksum) + 508 B of nibbles = 1016
  samples = 23.04 ms, 22,223 B/s. Playback is read-block → table-lookup
  decode → PCM to Bluedroid, which is far cheaper than SBC decode and keeps
  the spirit of the locked "no DSP at playback" decision.
- Blocks are self-contained, so **seeking is free** — track skip,
  pause/resume and the reel position display all depend on it.
- **Slot size 5 MiB → 5.125 MiB** (82 erase blocks) to preserve the full
  four-minute take at ADPCM's higher bitrate. Flagged deviation from the
  locked "3 x 5 MB slots"; keeps that decision's substance (three fixed
  slots, no dynamic allocation) and the 4-minute promise. 576 KiB spare.
- Retires M6 risk #1 (SBC-encode-while-capturing timing) — we no longer
  encode SBC at all.
- Crash recovery now validates a **payload checksum** per block. Without it
  a torn write whose header landed but whose data was cut short was
  wrongly accepted, keeping 23 ms of noise — a real bug the yank sweep
  caught once the format changed.
- Host tests: added `test_adpcm_block` (block geometry, capacity,
  independent decodability, truncation rejection). 5 suites, ~4,100
  assertions, green.

### Added (2026-08-25, firmware Phase A — M0/M1/M2)

- `firmware/` — ESP-IDF project skeleton: pin map (`main/include/board.h`),
  `sdkconfig.defaults` (Classic BT + A2DP source, BLE off, BR/EDR-only
  controller), component/CMake wiring, and a UI-bring-up `app_main`.
- `firmware/components/tape` — crash-safe slot manager behind an abstract
  `flash_hal.h`: append-only ping-pong header log (64-byte records, CRC32 +
  generation counter), just-in-time erase ahead of the write pointer, and
  mount-time repair of a slot left mid-recording.
- `firmware/components/ui` — button grammar and device state machine
  (REC hold, PLAY short/long, TRACK cycle, MODE 2/5/10 s), pure logic.
- `firmware/test/host` — NOR flash mock that models erase/program bit
  semantics and tears any chosen operation, plus a **power-yank sweep**
  that tears every flash operation of a recording in turn and re-checks the
  invariants. 4 suites, ~2,300 assertions, all passing in <1 s.
- `.github/workflows/firmware.yml` — CI: host tests via ctest, plus an
  ESP-IDF v5.3 container build.
- `docs/storage-budget.md` — the M1 arithmetic.

### Changed / findings (2026-08-25, firmware)

- **Record pipeline changed to one-pass SBC — needs Steven's sign-off.**
  The brief's two-pass pipeline needs a 5.05 MiB ADPCM scratch region, but
  three 5 MiB slots plus a header leave only 960 KiB — a 4.11 MiB deficit,
  i.e. 44 seconds of scratch instead of 4 minutes. Encoding SBC directly at
  capture removes the scratch entirely, honours the LOCKED "3 x 5 MiB slots"
  layout exactly, raises the affordable bitpool from 18 to 26 (121 -> 165
  kbps), halves flash writes, removes the post-record wait, and makes a
  power yank *keep* the partial take. Only loss is whole-take peak
  normalisation, replaced by a look-ahead limiter. ADPCM is implemented and
  tested regardless. See `docs/storage-budget.md` §4.
- Slot-repair scanning is a linear forward scan, not a binary search: a torn
  erase leaves a half-erased block whose tail still holds the previous take,
  so the slot is not monotonic and a binary search could resurrect stale
  audio. Caught by the block-crossing yank sweep.
- `tape_store_erase_slot` disowns a slot's audio *before* erasing it, so a
  yank mid-erase cannot leave the header claiming playable audio over
  half-erased flash.
- Storage-budget table corrected to whole-frame sizes (4:00 at bitpool 26 is
  4,961,220 B, not the fractional 4,961,250) — caught by `test_sbc_frame`.

### Added (2026-08-25, prior art research)

- `docs/prior-art.md` — eleven cassette-form-factor projects researched:
  Mixtape Alpha and 8Bit Mix Tape (direct ancestors), plus Mixxtape (Mixxim),
  the Larry Schotz car cassette adapter, 8BitMixtape NEO, JamHamster's ZX
  Spectrum cassette, the Cassette Pi IoT scroller, the **Digisette Duo-Aria
  (2000)** — the only known cassette-shaped device with record + playback,
  incl. deck playback via edge transducer, predating the Mixxim patent —
  the ION Bluetooth cassette adapter (mic inside the shell), Milktape /
  Suck UK USB mixtapes, and NFC mixtape tokens. Per-project takeaways and
  cross-cutting patterns.

### Added (2026-08-25, KiCad project setup)

- KiCad 9 project in `hardware/` (`mixxtape.kicad_pro`, `mixxtape.kicad_sch`,
  lib tables) with first-pass schematic: USB-C + ESD + CH340N, AMS1117 power,
  ESP32-WROOM-32E wired per the brief's GPIO map, I²S mic, W25Q128 flash,
  4 buttons, write-protect tab, castellated header, 29-LED WS2812 chain,
  and the DNP battery section. ERC passes with 0 errors.
- Symbols/footprints/3D models for all BOM parts fetched from JLCPCB/EasyEDA
  into `hardware/parts/mixxtape_parts.*` via `hardware/parts/fetch_parts.py`
  (uses curl_cffi Chrome TLS impersonation — EasyEDA's API blocks plain
  clients, and rate-limits bursts).
- Hand-authored `mixxtape_local.kicad_sym` (break-off tab, castellated
  header, JST connector symbols).
- `hardware/scripts/gen_schematic.py` — one-shot generator that produced the
  initial schematic (do not re-run after hand-editing in KiCad).

### Changed / findings (2026-08-25)

- **WS2812B-2020 substituted:** genuine Worldsemi part (C965555) is EOL at
  JLC (stock 3). Fitted part is XINGLIGHT XL-2020RGBC-2812B (**C5349955**,
  ~$0.06 @ 500, 180k stock) — same protocol and land pattern; schematic uses
  the WS2812B-2020 symbol/footprint with LCSC field C5349955.
- **CH340N has no DTR** (RTS# only) — the BOM's S8050 auto-reset pair needs
  both signals. Schematic fits a cap-coupled RTS reset instead; Q1/Q2 omitted
  until Steven decides (alternative: CH340C/CH343). Flagged on the schematic.
- LDO uprated to 1 A as the BOM required: AMS1117-3.3 (C6186), SOT-223.
- **USB-C is now power-only; programming moves to a jig (Steven,
  2026-08-25):** CH340N, USBLC6 ESD, and their caps removed; USB data pins
  NC. The castellated hacker header is removed. New J5: six bare pogo pads
  (3V3, GND, EN, IO0, TX, RX) — the jig carries the USB-UART bridge and
  auto-program transistors. Saves ~$0.60/board and the castellated-edge
  surcharge; resolves the CH340N-has-no-DTR question by removing it.
- **USB-C CC sensing added (2026-08-25):** CC1/CC2 wired through 10 k
  (R10/R11) to ADC1 on GPIO36/39 so firmware can read the source's current
  advertisement and raise the LED brightness cap on 1.5 A/3 A supplies.
  Extra resistors alone cannot unlock more current — the source advertises,
  the sink measures.
- **Firmware framework locked: ESP-IDF (Steven, 2026-08-25).** Zephyr was
  considered and rejected — BLE-first stack, partial BR/EDR host, upstream
  deprecation proposal for Classic BT, and no supported A2DP path on ESP32;
  Bluedroid (ESP-IDF) has the proven A2DP source + SBC encoder.
- **Name change planned (Steven, 2026-08-25):** "mixxtape" collides with
  Mixxim's commercial product name; will be renamed before going public.
- `docs/firmware-plan.md` — firmware architecture and milestone plan for
  agent handoff: task layout, two-pass record pipeline, yank-safety rules,
  no-devkit de-risking order (host-tested core + early 2–3 board prototype
  as the bring-up vehicle), factory-test firmware, CC-sense power logic.
  Flags a storage-budget conflict (4-min ADPCM scratch ≈ 5.3 MB vs ~1 MB
  spare) to resolve on paper first.
- **MicroSD footprint added, DNP (Steven, 2026-08-25):** TF-01A push-push
  socket (**C91145**, ~$0.13 @ 500) as J4, the "studio edition" option for
  loading songs. SPI-mode wiring sharing VSPI with the audio flash:
  CS = IO22 (with DNP 10 k pullup R9 so a populated card stays deselected
  during flash access), card-detect = IO32 (needs internal pullup in
  firmware), DAT1/DAT2 unused. Default build unchanged — footprint only.
- **LED level shifter added (Steven, 2026-08-25):** XL-2020RGBC-2812B
  confirmed working from Steven's past projects, including at 3.3 V data —
  but a TI SN74AHCT1G125 (**C7484**, SOT-23-5, ~$0.08) is fitted anyway as
  U8: ESP32 IO27 (`LED_DATA`) → buffer → `LED_DATA_5V` → D1, OE̅ tied to GND,
  powered from VBUS.

### Added

- Project write-up covering concept, rationale, and open design questions
  (`docs/cassette-project-writeup.md`)
- Agent handoff brief with locked decisions, GPIO map, behaviour spec, and
  suggested phase order (`docs/cassette-recorder-agent-brief.md`)
- Bill of materials v3 with LCSC part numbers and qty-20 costing
  (`docs/cassette-recorder-bom-v3.md`)
- Empty `firmware/` and `hardware/` directory placeholders
- README, changelog, and AGENTS.md

### Design decisions (as of 2026-08-25)

- MCU locked to ESP32-WROOM-32E for Classic Bluetooth / A2DP source
- USB-C only power; battery circuit on board but unpopulated
- 29 × WS2812B-2020 indicators replacing earlier charlieplex approach
- LDO to be uprated from 500 mA to 1 A (part selection pending)
- Write-protect tab count (one vs three) still open

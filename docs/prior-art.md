# Prior art — the cassette as a form factor

Research notes for the Mixxtape cassette recorder. Two direct ancestors, then
five other projects that use the compact cassette as the *shape* of the
device — not just as media. Each entry ends with what it teaches this project.

---

## Direct ancestors

### 1. Mixtape Alpha — Open Music Labs + Jie Qi (MIT Media Lab), 2012

<http://www.openmusiclabs.com/projects/mixtape_alpha/>

"The smallest synthesizer we could make without a prescription." An
ATmega328p synth on a cassette-shaped bare PCB, shipped in a real cassette
case with printed instructions. Stylophone-style continuous input plus 6
buttons; 4 voices, 4 effects, 5-note polyphony. You can record songs and
trade mixtapes with friends.

Fully open: schematics, code, board files, and even a Pure Data patch for
building custom wavetables live on the wiki
(`wiki.openmusiclabs.com/wiki/MixtapeAlpha`).

Design philosophy, in their own words: an attempt to break down the barriers
between people and electronics — to get them comfortable touching PCBs and
change expectations about how electronics should look. Resistive touch pads,
and all trace routing done with aesthetics as a first-class constraint.

**What it teaches us:** the entire design ethos (bare PCB as object, routing
as art), the cassette outline board file (`mixtape2.brd` — pull it rather
than redrawing), and the social loop: the device's output is tradeable.
Their answer to "what's on the tape" is clean — the device *makes* the
sound, so what's recorded exists nowhere else.

### 2. 8Bit Mix Tape — GaudiLabs / Dusjagr et al., ~2016

<https://www.hackster.io/news/the-8bit-mix-tape-d523f35405b7>

An Arduino-compatible ATtiny84/85 sound generator built to live *inside* a
real cassette shell, with control knobs poking out through the two reel
windows. Evolved publicly on its wiki from v0.1 (board strapped to the
outside of a dummy tape) to the v0.9 "Next Level Edition"; a "Pro Advanced
2000" variant was built into an NES cartridge.

**What it teaches us:** the reel windows are usable interface real estate —
they put knobs where we put LED rings. Also a lesson in iterating in public:
every board revision is on the wiki.

---

## Five more cassette-form-factor projects

### 3. Mixxtape — Mixxim, 2017 (commercial)

- Kickstarter: <https://www.kickstarter.com/projects/mixxtape/mixxtape-the-cassette-reinvented>
- Coverage: <https://newatlas.com/mixxtape-cassette-tape-music-player/49966/>,
  <https://www.hackster.io/news/mixxtape-turns-the-old-cassette-into-a-new-digital-music-player-2f362d95bf05>

The commercial cassette-shaped digital music player — 1215% funded in 30
days. MP3/FLAC playback, 8 GB onboard plus microSD to 256 GB, OLED display,
Bluetooth, rechargeable battery, headphone jack — and it physically plays
in a real tape deck through a tape-head interface (with the caveat that
decks with tape-movement sensors reject it).

**What it teaches us:** demand for the *object* is proven, and playback-only
is the ceiling of what's shipped — nobody has shipped recording. Also the
hazard: Mixxim holds a patent covering cassette-shaped players that play in
real decks. Tape-head playback stays deferred without a legal read
(brief §7).

### 4. Car cassette adapter — Larry Schotz, 1980s (commercial)

- <https://en.wikipedia.org/wiki/Cassette_tape_adapter>
- History: <https://www.vice.com/en/article/the-car-cassette-adapter-was-an-unsung-hero-at-the-dawn-of-the-digital-age/>

The original electronics-in-a-cassette-shell, invented by serial audio
inventor Larry Schotz to pipe a Discman into car tape decks. A "dummy"
tapeless cassette with a *write* head pressed against the deck's read head:
the incoming 3.5 mm signal is converted to a magnetic signal at the head
gap, no tape involved. One-way gears inside simulate reel movement so
auto-reverse and end-of-tape sensors don't eject it.

**What it teaches us:** this is the century-old prior art for tape-head
audio injection (relevant context for the Mixxim patent question), and its
fake-reel-motion gears are the mechanical checklist for what real decks
sense — exactly the sensors Mixxtape's deck-play mode would need to fool.

### 5. 8BitMixtape NEO — 8BitMixtape community, 2018

- <https://github.com/8BitMixtape/8Bit-Mixtape-NEO>
- <https://www.synthtopia.com/content/2018/05/15/8bit-mixtape-neo-synthesizer-an-open-lo-fi-electronic-music-platform/>

The 8Bit Mix Tape lineage matured into a platform: ATtiny85, 2 pots,
2 buttons, and 8 addressable NeoPixels on a cassette-styled PCB. Its party
trick is the TinyAudioBoot audio bootloader — new firmware is uploaded by
*playing a .wav file* into the board from any phone, computer, or Walkman.
The community shares "mixtapes" (firmware images as audio) that turn the
synth into a drum machine, pitch shifter, or algorave instrument.

**What it teaches us:** two things we're already circling — addressable RGB
LEDs on a cassette PCB read as personality, and firmware/content delivered
*as audio* is a proven, delightfully on-theme trick (worth remembering for
the dubbing feature: board-to-board transfer over sound is precedented).

### 6. ZX Spectrum in a cassette — JamHamster, 2021

- <https://www.tomshardware.com/news/raspberry-pi-zero-zx-spectrum-casette>
- <https://www.hackster.io/news/this-cassette-tape-emulates-the-zx-spectrum-61b6c23a33a3>

A Raspberry Pi Zero W physically trimmed (PCB sections cut off) to fit
entirely inside a genuine cassette shell, emulating the ZX Spectrum — the
computer whose software famously shipped *on* cassettes. Composite video out
via the Pi's test pads, PWM audio over GPIO, micro-USB power, and two USB-A
ports squeezed in for controllers.

**What it teaches us:** the shell's interior volume budget, proven the hard
way. Also the poetry angle: the cassette that *is* the computer instead of
holding its programs rhymes with our cassette that *is* the recording
device. (JamHamster later did the same with a working tape-head interface in
a TZXDuino build — worth a look if deck playback ever revives.)

### 7. Cassette Pi — IoT notification scroller, 2020

- <https://www.cnx-software.com/2020/08/16/using-an-old-cassette-tape-to-enclose-a-battery-powered-raspberry-pi-zero/>

A Raspberry Pi Zero W, battery, and LED matrix display inside a transparent
cassette shell. It receives IoT notifications via IFTTT; on message arrival
the cassette vibrates and scrolls the text across the LED display seen
through the shell.

**What it teaches us:** a transparent shell plus internal light is a strong
look — informative and glanceable without a screen. Confirms a battery and
radio fit inside the shell with room for a display, and that the cassette
silhouette makes even a notification gadget feel like an artifact.

---

## Round 2: record/playback devices in the cassette shell

A second research pass looking specifically for devices that *record* as well
as play, in cassette form factor.

### 8. Digisette Duo / Duo-Aria — Digisette LLC, 2000 ⭐

- Review: <https://www.soundandvision.com/content/quick-takes-digisette-duo-aria-mp3-player>
- Retrospective: <https://www.youtube.com/watch?v=19_OMV2P33I>
- FCC filing: <https://fcc.report/FCC-ID/PCMAR200/130275.pdf>

The closest thing to a true ancestor, seventeen years before Mixxtape: an
"e-cassette" MP3/WMA player in exact cassette form. Insert it whole into a
tape deck and an electromagnetic transducer along its edge lines up with the
deck's pickup head. 32–64 MB onboard plus MMC expansion, NiMH battery (~6 h
music / 9 h spoken word), USB dock — **and a digital voice recording
module.** A cassette-shaped device that records audio and plays it back
through a real tape deck shipped commercially in 2000, then vanished.

**What it teaches us:** (a) record + playback in the shell has been done
once, as a dictaphone feature — voice memos for yourself, not recordings you
trade, so the mixtape social loop is still unclaimed; (b) it's prior art
from 2000 for cassette-shaped players with deck playback, which reframes the
Mixxim patent question — worth handing to whoever does the legal read;
(c) even in 2000 a mic, battery, dock connector, and transducer all fit the
shell.

### 9. ION Audio Cassette Adapter Bluetooth — 2014 (commercial)

- <https://www.amazon.com/ION-Audio-Cassette-Adapter-Bluetooth/dp/B00I3YLHAC>
- <https://www.bhphotovideo.com/c/product/1049238-REG/ion_audio_bluetooth_cassette.html>

The Schotz adapter concept updated: a Bluetooth receiver in a cassette shell
feeding the deck's head, with a rechargeable battery (~6 h) — and a
**built-in microphone** for hands-free calls through the car speakers. Turns
itself on when inserted and off when ejected.

**What it teaches us:** a commercial product already put a working microphone
inside a cassette shell in an acoustically hostile spot (inside a running
tape transport) — encouraging for our handling-noise worry, though our
bare-PCB mic has no shell around it. The insert-to-power-on trick is also a
nice interaction precedent.

### 10. Milktape and Suck UK "Mix Tape" — cassette-shaped USB mixtapes

- <https://milktape.com/>
- <https://www.suck.uk.com/products/mixtapeusbdrive/>

Cassette-shaped USB flash drives sold explicitly as mixtape gifts. Suck UK's
comes in a cassette case with a paper J-card to handwrite the track list;
Milktape sells small-capacity (128 MB) customizable tapes — deliberately
sized to hold *one mixtape*, not a music library.

**What it teaches us:** the gift ritual sells even with zero audio hardware —
the shell, the case, and the handwritten label carry the whole product.
Milktape's small-on-purpose capacity validates our 3-track constraint as a
feature, not a limitation. Both are also prior art for the handwritten-label
ritual our matte-white sharpie block serves.

### 11. NFC mixtape cassettes — Etsy makers & DIY, 2020s

- <https://makerworld.com/en/models/1077383-retro-nfc-cassette-keychain-for-spotify-playlist>
- <https://www.notebookcheck.net/New-3D-printed-cassette-player-nails-the-retro-aesthetic-using-NFC-and-Spotify.1154678.0.html>

A living cottage industry of cassette-shaped NFC tokens: tap the cassette to
a phone and a Spotify playlist opens. One maker (bharms27) built a full
3D-printed "player" — a phone dock where inserting a printed cassette with an
NFC tag starts the album, complete with spinnable rollers.

**What it teaches us:** demand for *physical playlist tokens* is current and
ongoing, not just 80s nostalgia — but every one of these outsources the
audio to a streaming service. The token is a pointer, the music dies with
the subscription. Our tape holding the actual audio is the durable version
of the same desire.

---

## Patterns across all eleven

- **Recording is almost untouched.** Only the Digisette (2000) ever put
  record + playback in the shell, and only as a self-dictation feature.
  Nobody has shipped *recording as the point* — record, hand it over, snap
  the tab. The niche is still empty.
- **The reel windows are the signature feature** — knobs (8Bit Mix Tape),
  visible mechanism (adapters), spinnable rollers (NFC player), or our LED
  rings. Designs that ignore them read as generic rectangles.
- **The gift ritual is the product** for a whole segment (Milktape, Suck UK,
  NFC tokens) — shell + case + handwritten label sell without any audio
  hardware at all.
- **Open + tradeable beats closed + polished** in this niche's culture: the
  projects with living communities (Mixtape Alpha, 8BitMixtape) are the
  fully open ones.
- **Audio as a transport layer** keeps recurring (audio bootloader, tape-head
  injection) and is on-theme for dubbing.
- **Deck compatibility is a patent minefield** (Mixxim) — but the 1980s
  Schotz adapters and the 2000 Digisette both predate it; a legal read has
  real prior art to work with. Stays deferred regardless.
- **Mics survive inside cassette shells** (ION adapter, Digisette) — the
  open question unique to us is a mic on a *bare* PCB with no enclosure.

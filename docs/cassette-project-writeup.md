# Building a cassette you can actually record on

I've been designing a PCB shaped like a cassette tape. Not a case with
electronics inside — the bare board *is* the cassette. Same 100 × 63.5 mm
outline, reel windows routed out, and it drops into a real Norelco case (I'm
buying those at thrift stores).

It has a microphone. You hold down REC and it records. Three tracks. Then you
pair it to any Bluetooth earbuds or speaker and it plays them back.

That's the whole thing.

---

## Why this and not the ten cassette MP3 players that exist

There are cassette-shaped Bluetooth players you can buy — the Mixxtape is the
good one, ~$100, plays in real tape decks. But they're all **players**. You
sideload files onto them.

Nobody ships a cassette-shaped **recorder**. And recording is the part that made
a mixtape a mixtape. You made it, it was yours, you gave it to someone.

The other ancestor here is the Mixtape Alpha from Open Music Labs — a
cassette-shaped ATmega328p synth from 2012, done with Jie Qi at the MIT Media
Lab. Their whole point was getting people comfortable touching a bare PCB.
Fully open, files still up. I'm stealing that philosophy wholesale.

---

## How you'd use it

- **Hold REC** — records. Let go, it stops. Holding always overwrites from zero,
  so recording *is* erasing, same as tape.
- **PLAY** — plays all three tracks in order. It's a tape side, not three voice
  memos.
- **TRACK** — skip.
- **MODE, 2 seconds** — pairing. Hold the board against your speaker while its
  pairing light is blinking. It picks the closest device by signal strength, so
  proximity is the selection mechanism. No phone, no app, no list.

Plug it in and it auto-reconnects to the last thing it paired with.

---

## The bits I'm happiest with

**A write-protect tab that actually snaps off.** Break-off PCB tab on the top
edge, mouse-bitten, carrying a trace to a GPIO. Snap it and the firmware
refuses to record. Permanently. It's the "don't tape over my mixtape" thing,
and it makes handing someone a tape into a ritual with a physical, irreversible
last step.

**The reels do something useful.** Twelve RGB LEDs in each reel window. As a
track plays, the left ring empties and the right ring fills — tape transferring
between reels — so it's a position indicator nobody has to learn. Red pulse for
recording with brightness tracking level. Slow reverse spin while it's erasing a
slot in the background, which is what a real deck does before you record over
something.

**Matte white solder mask so you can sharpie the label on.** Glossy green mask
rejects permanent marker — it beads and thumbs off. Matte white takes it.

**No battery.** This is the decision I keep going back and forth on. A LiPo in a
drawer for eight years is dead and possibly puffed. A cassette's whole promise
is that you find it years later and it still works. So: USB-C only, with the
battery footprint on the board but unpopulated for anyone who wants to solder
one on. A power bank in your pocket with a cassette on a cable is basically a
Walkman anyway.

---

## Boring but load-bearing decisions

- **Original ESP32**, not an nRF or an ESP32-S3/C3. Only the old Xtensa ESP32
  has Classic Bluetooth, and without Classic Bluetooth there's no A2DP, which
  means it can't talk to normal earbuds. LE Audio would work but Apple still
  doesn't support it, so half of you couldn't use it.
- **Mono mic.** One microphone, so, mono. Encodes to SBC while you record so
  playback is just read-bytes-and-transmit — no real-time DSP.
- **Separate flash chip** for audio so a yanked USB cable mid-record can't
  brick the firmware.
- ~$13/board at a run of 20. NOR flash prices have roughly tripled this year
  which is genuinely annoying and drove several of the sizing decisions.

---

## Where I actually want your input

**1. Is a microphone recording of a room a thing anyone wants to trade?**
This is my real doubt. Mixtape Alpha had a clean answer — the device *makes*
the sound, so what's on it exists nowhere else. Mine records whatever's nearby,
which might just be a worse copy of something that already exists. Is the
answer "voice messages, not music"? Is it "ambient recordings are the point"?
Or is it "this needs a line-in and you're kidding yourself"?

**2. Battery or no battery?** Argue me out of the no-battery position.

**3. One write-protect tab for the whole tape, or three (one per track)?**
Three is more useful. One is more faithful.

**4. How bad is holding a bare PCB with a mic on it going to sound?**
Fingernails, board flex, handling noise straight into a MEMS mic. This is my
biggest technical worry and I don't think I can answer it without building one.
If anyone's fought this, I want to hear it.

**5. What do I call it?**

**6. Do you want one?** Making ~20. Some will be bad.

# The manual

> **Draft.** No hardware exists yet. This describes the design as built in
> firmware and schematic; anything marked *(unverified)* is waiting on the
> first prototype boards. The product name is still undecided — this
> document says "your tape" throughout, which is what it is anyway.

---

## What this is

A circuit board shaped exactly like a cassette. Not a case with
electronics inside — the bare board *is* the cassette. It has a
microphone. You hold down REC and it records. Then you pair it to any
Bluetooth earbuds or speaker and press PLAY.

Three tracks, about four minutes each. One side. That's the whole thing.

It drops into an ordinary cassette case, and the board is matte white so
you can write the label on it in permanent marker, the way you used to.

---

## What you need

- **A USB-C cable and something to power it from.** A phone charger, a
  laptop, a power bank. There is no battery — see
  [Power](#power-and-batteries).
- **Bluetooth earbuds or a speaker.** Anything that ordinary phones pair
  with will do.

---

## The controls

Four buttons along the bottom edge, and a small tab on the top edge.

| Button | Press | Hold |
|---|---|---|
| **REC** | — | Records for as long as you hold it |
| **PLAY** | Play / pause | Hold ~1 s to sleep |
| **TRACK** | Next track (1 → 2 → 3 → 1) | — |
| **MODE** | — | 2 s: pair · 5 s: dub · 10 s: developer mode |

MODE decides what to do when you *let go*, so holding past two seconds
does not accidentally start pairing on the way to something else. The
lights tell you which one you are about to get.

---

## Getting started

**1. Plug it in.** The reels wake up. If it has paired with something
before, it starts looking for it straight away.

**2. Pair it.** Put your speaker or earbuds into their own pairing mode,
then hold **MODE for two seconds**. The reels sweep amber while it looks
around. Now hold your tape against the speaker.

That last part is the whole interface. Your tape picks the closest audio
device it can find, so proximity is how you choose. There is no phone, no
app, and no list of devices to scroll through. It ignores phones,
laptops and anything else that is not a speaker, so the only thing you
can accidentally pair with is another speaker further away — and the fix
for that is to move closer and try again.

The reels turn green when it connects.

**3. Record.** Hold **REC**. The reels pulse red, brighter as the room
gets louder. Let go and it stops.

**4. Play.** Press **PLAY**. It plays all three tracks in order, like a
side of a tape. The left reel empties and the right reel fills as it goes.

---

## Recording

**Hold REC to record. Let go to stop.** There is no separate stop button
and no record-arm step.

**Recording erases.** Holding REC always records over the current track
from the beginning — it does not append, and there is nothing to delete.
This is exactly how tape worked, and it is deliberate: the only way to
keep something is not to record over it.

**Each track holds about four minutes.** If you fill one, recording stops
there. Press TRACK to move to the next one.

**Three tracks, about twelve minutes in total.** That is not a limitation
we are apologising for. A mixtape that holds everything is a hard drive.

**If the power is pulled while you are recording, you keep what you
recorded.** The take is simply shorter than it would have been. Nothing
else on the tape is affected. This matters more than it sounds, because
pulling the cable is how this thing switches off.

### Levels

You do not set a recording level and there is no gain knob. Your tape
watches the room and adjusts continuously: quiet rooms get lifted so
ambience is audible, and sudden loud sounds are caught before they
distort. The REC lamp brightness follows the input, so you can see it
working.

---

## Playing

**PLAY plays the whole side** — all three recorded tracks, in order,
without gaps between them. Empty tracks are skipped rather than played as
silence.

- **PLAY again** pauses. Press again to carry on.
- **TRACK** jumps to the next recorded track.
- **Hold PLAY** for about a second to put it to sleep. Any button wakes it.

While it plays, the **left reel empties and the right reel fills**. That
is your position on the side. Nobody should have to be told what it means,
which is the point.

---

## The lights

Twenty-four lights ring the two reel windows, and five more sit along the
bottom: three for the track, one for record, one for Bluetooth.

| What you see | What it means |
|---|---|
| Amber sweeping round the reels | Looking for a speaker |
| Reels turn green | Connected |
| Left reel emptying, right filling | Playing — this is your position |
| Red pulse, brightness following the room | Recording |
| Slow, dim reverse spin | Tidying up in the background. Ignore it |
| Violet | Copying to another tape |
| Slow red pulse after ~15 seconds | The speaker is busy — see below |
| Dark | Asleep, or nothing happening |

**The slow red pulse** almost always means the speaker or earbuds are
still connected to someone's phone. Bluetooth devices only talk to one
thing at a time. Disconnect it at the other end, or turn the phone's
Bluetooth off, and your tape will get in.

---

## The write-protect tab

There is a small tab on the top edge, joined by a few tiny bridges. It
does the same job as the plastic tab on a real cassette.

**Snap it off and your tape can never record again.** Not "until you
reset it" — never. It is a physical trace across a break line; once it is
gone, the firmware refuses to record, permanently.

This is the point of the object. You record something, you write the
label on the front, you snap the tab, and you hand it to someone. The last
step is irreversible and takes a physical act, which is what makes it a
gift rather than a file transfer.

Snap it with your fingernail or a pair of pliers. Everything else keeps
working — it plays exactly as before.

**Do not snap it if you are still deciding.** We cannot undo it, and
neither can you.

---

## Power and batteries

**There is no battery. It runs from USB-C only.**

That is a deliberate choice, and the reason is the whole premise of the
object. A lithium battery left in a drawer for eight years is dead, and
possibly swollen. A cassette you find in a box years later should still
work. So: no battery, nothing to degrade, nothing to leak.

In practice a small power bank in your pocket with a tape on a cable is
more or less a Walkman.

Any USB-C source will run it. If you plug it into a charger that can
supply more current, it notices and runs the lights brighter.

There is no battery on the board and nowhere to fit one. The charging
circuit that used to sit there unpopulated has been removed: it could never
have worked, because the 3.3 V regulator needs more headroom than a lithium
cell can give it.

---

## Labelling it

The board is matte white on purpose. Glossy circuit boards reject
permanent marker — it beads up and rubs off on your thumb. Matte takes it
and keeps it.

There are faint ruled lines in the upper-left area, where a cassette's
paper label goes. Write on it. That is what it is for.

---

## Looking after it

It is a bare circuit board, and it is meant to be handled — but a few
things are worth knowing.

- **Keep it in the case.** It arrives in an ordinary cassette case. That
  is the best protection it will get.
- **There is a small hole under the microphone.** Do not cover it, block
  it with a sticker, or poke anything into it.
- **Do not get it wet**, and do not leave it somewhere very cold and then
  breathe on it.
- **Static** is the usual way to kill a bare board. Touch something
  earthed before picking it up in a very dry room. In normal conditions
  this is not something to worry about.
- **Handling noise.** It has a microphone on a bare board, so fingernails
  and flexing show up in the recording. Hold it by the edges while
  recording, or better, put it down. *(How much this matters is one of the
  things the first prototypes are meant to tell us.)*

---

## If something is wrong

**Nothing lights up.** Try a different cable — a surprising number of
USB-C cables are charge-only or simply broken. Then try a different port.

**It will not pair.** Make sure the speaker is in *its* pairing mode, not
just switched on. Hold the tape right against it. If the reels sweep amber
and then pulse red slowly, the speaker is talking to something else.

**It paired but there is no sound.** Check the speaker's own volume.
Press PLAY. If the tracks are empty there is nothing to play — the reels
will not move.

**It will not record.** Look at the top edge. If the tab is gone, that is
the answer, and it is permanent.

**A recording is shorter than expected.** Either you filled the track
(about four minutes) or power was interrupted. In both cases what was
recorded is intact.

**It sounds bad.** Try holding it by the edges, or setting it down
somewhere solid instead of holding it at all.

---

## For people who want to open it up

Everything is public: schematic, board files, firmware, and the design
notes that argue with themselves along the way. It descends from Open
Music Labs' Mixtape Alpha, and it inherits that project's attitude that
you should be comfortable touching a bare board.

There is no USB data connection — USB-C carries power only. Firmware is
loaded through six pads along the edge using a jig, which keeps the
production board simple. If you want to build one, the repository has the
details.

The board also has an unpopulated microSD footprint and an unpopulated
line-in jack, for anyone who wants to build a variant.

---

## Specifications

| | |
|---|---|
| Size | 100.0 × 63.5 mm, 1.6 mm thick — a cassette |
| Recording | 3 tracks, about 4 minutes each |
| Audio | Mono, 44.1 kHz, one built-in microphone |
| Storage | 16 MB, on board, no card needed |
| Wireless | Bluetooth Classic (A2DP) — ordinary earbuds and speakers |
| Paired devices remembered | 8, most recent tried first |
| Power | USB-C only. No battery |
| Case | Standard cassette case |

---

## What this does not do

Worth being straight about, because people ask:

- **It does not play in a tape deck.** It is cassette-shaped, but there is
  no tape head, and putting it in a deck will do nothing.
- **It is not a music player.** You cannot load MP3s onto it. It records
  what is in front of it. That is the entire idea — plenty of
  cassette-shaped players already exist, and none of them record.
- **There is no app**, and there will not be one.
- **It does not connect to your phone as a phone accessory.** It talks
  *to* speakers, the way a phone does.

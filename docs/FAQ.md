# Frequently asked questions

> **Draft.** No hardware exists yet, so answers marked *(unverified)* are
> design intent waiting on the first prototype boards. Where something is
> genuinely not known, this says so rather than guessing.

See also: [the manual](MANUAL.md).

---

## The basics

### What is it?

A circuit board shaped exactly like a cassette, with a microphone on it.
Hold REC to record, pair it to Bluetooth earbuds or a speaker, press PLAY.
Three tracks, about four minutes each.

### Does it play in a real tape deck?

**No.** It is cassette-shaped, but there is no tape head. Put it in a deck
and nothing happens.

This is deliberate rather than an oversight. Making a cassette-shaped
device that plays through a real deck is covered by a patent held by
Mixxim, who make the Mixxtape player. We are not going anywhere near that
without a lawyer reading it first, and it is not what this device is for
anyway.

### So it is another cassette Bluetooth player?

No — and this is the whole reason the project exists. There are perhaps
ten cassette-shaped Bluetooth *players*. You sideload files onto them.

Nobody ships a cassette-shaped **recorder**, and recording is the part
that made a mixtape a mixtape. You made it, it was yours, you gave it to
someone.

We looked. In [eleven cassette-form-factor projects](prior-art.md) going
back to the 1980s, exactly one device ever recorded — the Digisette
Duo-Aria in 2000, and only as a dictaphone feature. Record-as-the-point
is still an empty niche.

### Can I put my own music on it?

**No.** There is no way to load audio files onto it, by design.

It records what is in front of the microphone. If you want to put an
album on a cassette-shaped object, the Mixxtape player already does that
well.

### Can I get my recordings off it, onto a computer?

Not in the default build. There is no USB data connection — USB-C carries
power only.

This is a real limitation and we know it. It exists because dropping the
USB data path removed a chip, saved board space, and closed off a whole
class of "why won't my computer see it" problems. The recordings are meant
to live on the tape and be given away with it.

If you want this, the board has an unpopulated microSD footprint. See
[Building your own](#building-your-own).

---

## Recording

### How long can I record?

About four minutes per track, three tracks, so roughly twelve minutes in
total.

That is not a compromise we are embarrassed about. A mixtape that holds
everything is a hard drive.

### Can I record over a track?

Yes — that is the only way it works. Holding REC always records over the
current track from the beginning. There is no append, and no delete
function, because **recording is erasing**, exactly like tape.

### Can I record more than one thing onto a single track?

No. Each hold of REC is one take, and it replaces the whole track.

### How good does it sound?

Mono, 44.1 kHz, from a single small microphone. Think good voice memo
rather than field recording.

*(Unverified.)* The genuine open question is handling noise: it is a
microphone on a bare circuit board, so fingernails and board flex go
straight into the recording. We have designed around it — relief slots
near the mic, placement away from where you naturally grip — but nobody
knows how well that works until a board exists. It is the project's
biggest technical risk and we are not going to pretend otherwise.

### Do I need to set a recording level?

No. There is no gain control. It watches the room continuously — lifting
quiet material so ambience is audible, and catching sudden loud sounds
before they distort. The REC lamp brightness follows the input so you can
see it working.

### What happens if the power is pulled mid-recording?

**You keep what was recorded.** The take is simply shorter. Nothing else
on the tape is affected, and the tape is immediately usable again.

This is not a happy accident. There is no battery, so pulling the cable is
the normal way of switching off, and the storage layer was built around
that from the start. Every single flash operation of a recording has been
tested by simulating a power cut at exactly that moment and checking that
existing recordings are still byte-identical afterwards.

### Can I record while it is playing?

No. Pressing REC stops playback and starts recording.

---

## The write-protect tab

### What does the tab do?

The same thing the plastic tab on a real cassette did. Snap it off and the
tape can never record again.

### Is it really permanent?

**Yes.** It is a trace on a break-off piece of the board. Once it is
snapped, the connection is gone, and the firmware refuses to record.
There is no reset, no button combination, and no way for us to undo it.

### Why would I want that?

Because it is what makes handing someone a tape into a thing that
happened. You record it, you write the label on the front, you snap the
tab, and you give it away. The last step is physical and irreversible.

A file you send someone is a copy. This is the object.

### Can I still play it after snapping the tab?

Yes. Playback, pairing and everything else work exactly as before. Only
recording is disabled.

### One tab for the whole tape, or one per track?

One for the whole tape. Three would be more useful; one is more faithful,
and matches what a cassette actually did. *(Still open — Steven has not
finally decided.)*

---

## Bluetooth and pairing

### What can I pair it with?

Ordinary Bluetooth earbuds and speakers — anything a phone pairs with for
music. It behaves like a phone, not like an accessory: it is the thing
sending the audio.

### Will it work with AirPods?

It should — AirPods are a standard Bluetooth audio sink. *(Unverified.
Apple devices are notoriously particular about pairing, and testing
against a range of real earbuds is explicitly on the list before any
boards are built in quantity.)*

### How do I choose which speaker to use?

You hold the tape against it. That is the entire interface.

When you hold MODE for two seconds it looks around, ignores everything
that is not a speaker or headphones — phones, laptops, watches, keyboards
— and picks the closest one. There is no list and no app.

### It keeps failing to connect and pulsing red slowly.

The speaker is almost certainly still connected to someone's phone.
Bluetooth audio devices talk to one thing at a time. Disconnect it at the
other end and try again.

The slow red pulse means exactly that, rather than a general failure. We
would rather tell you what is wrong than blink hopefully.

### Does it remember what I paired with?

Yes — up to eight devices, most recently used first. Plug it in and it
reconnects to the last one on its own. Those are stored in flash and
survive the cable being pulled.

### Why Bluetooth Classic and not the newer low-energy audio?

Because ordinary earbuds do not support the newer standard yet. LE Audio
would be a better fit technically, but Apple does not support it, so a
large fraction of people simply could not use the thing.

That decision drove the whole design: Classic Bluetooth is why this uses
the original ESP32 rather than a newer, smaller, cheaper one. Every newer
ESP32 is low-energy only.

---

## Power

### Why is there no battery?

Because a cassette should still work when you find it in a box years
later. A lithium cell left in a drawer for eight years is dead, and
sometimes swollen. Building that into an object whose entire premise is
"it still works when rediscovered" would be self-defeating.

It also avoids the shipping paperwork that lithium cells drag along.

### So I need to keep it plugged in?

Yes. A small power bank in a pocket, with the tape on a cable, is more or
less a Walkman.

### Can I add a battery myself?

Not without redesigning the power supply. The board used to carry an
unpopulated charger, but it was removed once it became clear it could never
have worked: the 3.3 V rail comes from an AMS1117, which needs 1.1-1.3 V of
headroom, and a lithium cell only ever reaches 4.2 V falling to 3.0 V. The
LEDs run from the 5 V USB rail as well. Running on a cell means a different
regulator and a different LED rail — a real redesign, not a part you add.

### Will any USB charger work?

Yes. If it is plugged into a supply that can give more current, it notices
and runs the lights brighter — otherwise it stays inside what a basic USB
port can provide.

---

## The object

### Is it safe to hold a bare circuit board?

Yes. There is nothing on it above 5 volts and nothing that gets hot.

The bare board is the point, not a cost saving. It descends from Open
Music Labs' Mixtape Alpha, whose stated goal was getting people
comfortable touching electronics and changing expectations about what
electronics should look like. We took that wholesale.

Static is the usual way to damage a bare board — touch something earthed
first in a very dry room. In normal conditions, do not think about it.

### Why is the board white?

So you can write on it. Ordinary green circuit boards are glossy and
reject permanent marker — it beads up and thumbs off. Matte white takes
it and keeps it, and there are faint ruled lines where a cassette's paper
label used to go.

### Does it come with a case?

Yes, an ordinary cassette case, mostly bought secondhand. The case is not
packaging; it is what makes it read as a cassette when you hand it over.

### What are all the lights for?

Twelve around each reel window. As a track plays the left ring empties and
the right ring fills — tape moving from one reel to the other. It is a
position indicator nobody has to learn.

They also show recording level, pairing progress, and the background
tidying-up that happens before a track can be recorded over.

---

## Building your own

### Is it open source?

Yes. Schematic, board files, firmware and the design notes are all public,
including the arguments that changed our minds along the way.

### Can I build one?

Yes. It is a two-layer board with parts on one side, priced at roughly $13
each in a batch of twenty. The repository has the bill of materials with
part numbers.

### What is the unpopulated microSD slot for?

A "studio edition" variant. The footprint is there, wired up and not
fitted, so anyone who wants removable storage can add it without
redesigning the board.

We left it off the default build on purpose. An SD card you load songs
onto turns this back into the cassette-shaped player that already exists,
and it weakens the tape metaphor — the write-protect tab means much less
when the media pops out.

### Can I write my own firmware for it?

Yes. There is no USB data connection, so firmware goes on through six pads
along the board edge with a jig. The repository explains it. The audio
storage, the playback sequencer, the reel display and the pairing logic
are all portable C with tests you can run on a PC without any hardware.

### What is it called?

Undecided. The repository is currently named after an existing commercial
product, which is a problem we intend to fix before this goes anywhere
public. Suggestions welcome.

---

## Honest limitations

Collected in one place, because they are easier to trust than to discover:

- **No way to get recordings onto a computer** in the default build.
- **Twelve minutes total**, and each track is all-or-nothing.
- **Mono**, from one small microphone.
- **Handling noise is an open risk** — a bare board with a mic on it is
  not the quietest thing to hold. *(Unverified until prototypes exist.)*
- **It needs to stay plugged in.**
- **It does not play in a tape deck**, and will not.
- **Reconnect time is unknown.** *(If it turns out to take eight seconds
  to reconnect on power-up, the whole interaction needs rethinking, and we
  will say so.)*

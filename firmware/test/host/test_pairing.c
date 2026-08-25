/* Pairing policy: the filters that make "hold it against the speaker" the
 * whole interaction, and the backoff that keeps it honest when the sink is
 * busy talking to someone's phone. */

#include <string.h>

#include "pairing.h"
#include "test_util.h"

/* Build a Class of Device from its parts. */
static uint32_t cod(uint32_t major, uint32_t minor, bool audio_service)
{
    uint32_t v = ((major & 0x1Fu) << COD_MAJOR_SHIFT) |
                 ((minor & 0x3Fu) << COD_MINOR_SHIFT);
    if (audio_service) {
        v |= COD_SERVICE_AUDIO;
    }
    return v;
}

static bt_device_t dev(const char *name, uint32_t c, int8_t rssi, uint8_t last)
{
    bt_device_t d;
    memset(&d, 0, sizeof(d));
    strncpy(d.name, name, PAIRING_NAME_LEN - 1u);
    d.cod = c;
    d.rssi = rssi;
    d.bda[5] = last;
    return d;
}

static void test_accepts_real_sinks(void)
{
    CHECK(pairing_is_audio_sink(cod(COD_MAJOR_AUDIO, COD_AV_HEADPHONES, true)));
    CHECK(pairing_is_audio_sink(cod(COD_MAJOR_AUDIO, COD_AV_LOUDSPEAKER, true)));
    CHECK(pairing_is_audio_sink(cod(COD_MAJOR_AUDIO, COD_AV_HEADSET, true)));
    CHECK(pairing_is_audio_sink(cod(COD_MAJOR_AUDIO, COD_AV_HANDSFREE, true)));
    CHECK(pairing_is_audio_sink(cod(COD_MAJOR_AUDIO, COD_AV_HIFI, true)));
    CHECK(pairing_is_audio_sink(cod(COD_MAJOR_AUDIO, COD_AV_PORTABLE, true)));

    /* Plenty of cheap speakers do not set the Audio service bit. Rejecting
     * them on that alone would be a support nightmare. */
    CHECK(pairing_is_audio_sink(cod(COD_MAJOR_AUDIO, COD_AV_HEADPHONES,
                                    false)));
}

static void test_rejects_everything_else(void)
{
    /* This is the filter that means the user never sees a list of their
     * own hardware. */
    CHECK(!pairing_is_audio_sink(cod(COD_MAJOR_PHONE, 0x03u, true)));
    CHECK(!pairing_is_audio_sink(cod(COD_MAJOR_COMPUTER, 0x03u, true)));
    CHECK(!pairing_is_audio_sink(cod(COD_MAJOR_PERIPH, 0x10u, false)));
    CHECK(!pairing_is_audio_sink(cod(COD_MAJOR_WEARABLE, 0x02u, true)));

    /* Same major class, but not things to play a mixtape into. */
    CHECK(!pairing_is_audio_sink(cod(COD_MAJOR_AUDIO, 0x04u, true))); /* mic */
    CHECK(!pairing_is_audio_sink(cod(COD_MAJOR_AUDIO, 0x09u, true))); /* STB */
    CHECK(!pairing_is_audio_sink(cod(COD_MAJOR_AUDIO, 0x0Bu, true))); /* VCR */
    CHECK(!pairing_is_audio_sink(cod(COD_MAJOR_AUDIO, 0x0Cu, true))); /* cam */
    CHECK(!pairing_is_audio_sink(cod(COD_MAJOR_AUDIO, 0x0Eu, true))); /* mon */
    CHECK(!pairing_is_audio_sink(cod(COD_MAJOR_AUDIO, 0x00u, true))); /* uncat */

    CHECK(!pairing_is_audio_sink(0u));
    CHECK(!pairing_is_audio_sink(0xFFFFFFu));
}

static void test_range_gate(void)
{
    CHECK(pairing_in_range(-40, PAIRING_RSSI_DEFAULT));
    CHECK(pairing_in_range(-50, PAIRING_RSSI_DEFAULT)); /* inclusive */
    CHECK(!pairing_in_range(-51, PAIRING_RSSI_DEFAULT));
    CHECK(!pairing_in_range(-90, PAIRING_RSSI_DEFAULT));
}

static void test_nearest_sink_wins(void)
{
    /* The scenario the product is built around: a room with a phone, a
     * laptop and two speakers, and the user is holding the board against
     * one of the speakers. */
    bt_device_t found[5] = {
        dev("Pixel", cod(COD_MAJOR_PHONE, 0x03u, true), -35, 1),
        dev("MacBook", cod(COD_MAJOR_COMPUTER, 0x03u, true), -38, 2),
        dev("Kitchen speaker", cod(COD_MAJOR_AUDIO, COD_AV_LOUDSPEAKER, true),
            -72, 3),
        dev("The one in my hand",
            cod(COD_MAJOR_AUDIO, COD_AV_LOUDSPEAKER, true), -41, 4),
        dev("Someone's earbuds",
            cod(COD_MAJOR_AUDIO, COD_AV_HEADPHONES, true), -66, 5),
    };
    int pick = pairing_select(found, 5u, PAIRING_RSSI_DEFAULT);
    CHECK_EQ(pick, 3);

    /* The phone is closer than every speaker and still must not win. */
    found[0].rssi = -20;
    CHECK_EQ(pairing_select(found, 5u, PAIRING_RSSI_DEFAULT), 3);
}

static void test_nothing_in_range(void)
{
    bt_device_t found[2] = {
        dev("Far speaker", cod(COD_MAJOR_AUDIO, COD_AV_LOUDSPEAKER, true), -70,
            1),
        dev("Phone", cod(COD_MAJOR_PHONE, 0x03u, true), -30, 2),
    };
    /* Better to find nothing than to pair with the wrong thing — the user
     * just moves the board closer and holds MODE again. */
    CHECK_EQ(pairing_select(found, 2u, PAIRING_RSSI_DEFAULT), -1);
    CHECK_EQ(pairing_select(found, 0u, PAIRING_RSSI_DEFAULT), -1);
    CHECK_EQ(pairing_select(0, 3u, PAIRING_RSSI_DEFAULT), -1);

    /* A looser threshold does let the distant speaker through. */
    CHECK_EQ(pairing_select(found, 2u, -80), 0);
}

static void test_selection_is_stable(void)
{
    /* Equal signal must not flip the choice between inquiries, or the
     * board would look like it is dithering. */
    bt_device_t found[2] = {
        dev("A", cod(COD_MAJOR_AUDIO, COD_AV_HEADPHONES, true), -45, 1),
        dev("B", cod(COD_MAJOR_AUDIO, COD_AV_HEADPHONES, true), -45, 2),
    };
    for (int i = 0; i < 10; i++) {
        CHECK_EQ(pairing_select(found, 2u, PAIRING_RSSI_DEFAULT), 0);
    }
}

/* --- reconnect backoff ----------------------------------------------- */

static void test_backoff_grows_and_caps(void)
{
    reconnect_t r;
    reconnect_init(&r, 1000u);

    CHECK(reconnect_should_try(&r, 1000u)); /* first try is immediate */

    uint32_t now = 1000u;
    uint32_t last = 0;
    for (int i = 0; i < 12; i++) {
        reconnect_failed(&r, now);
        uint32_t d = reconnect_delay_ms(&r);
        CHECK(d >= last || d == RECONNECT_MAX_DELAY_MS);
        CHECK(d <= RECONNECT_MAX_DELAY_MS);
        last = d;
        now += d;
    }
    CHECK_EQ(last, RECONNECT_MAX_DELAY_MS);
}

static void test_waits_between_attempts(void)
{
    reconnect_t r;
    reconnect_init(&r, 0u);

    reconnect_failed(&r, 0u);
    uint32_t d = reconnect_delay_ms(&r);
    CHECK(!reconnect_should_try(&r, d - 1u));
    CHECK(reconnect_should_try(&r, d));
}

static void test_success_stops_retrying(void)
{
    reconnect_t r;
    reconnect_init(&r, 0u);
    reconnect_failed(&r, 0u);
    reconnect_failed(&r, 100u);

    reconnect_succeeded(&r);
    CHECK(!reconnect_should_try(&r, 100000u));
    CHECK(!reconnect_sink_busy(&r, 100000u));
}

static void test_sink_busy_is_reported_honestly(void)
{
    /* After about fifteen seconds of failure the user is owed an answer:
     * the speaker is busy, not the tape broken. */
    reconnect_t r;
    reconnect_init(&r, 5000u);

    CHECK(!reconnect_sink_busy(&r, 5000u));
    CHECK(!reconnect_sink_busy(&r, 5000u + RECONNECT_BUSY_AFTER_MS - 1u));
    CHECK(reconnect_sink_busy(&r, 5000u + RECONNECT_BUSY_AFTER_MS));
    CHECK(reconnect_sink_busy(&r, 5000u + 60000u));
}

static void test_survives_clock_wrap(void)
{
    /* The millisecond counter wraps after ~49 days. A tape left plugged in
     * that long should not stop retrying. */
    reconnect_t r;
    uint32_t near_wrap = 0xFFFFFF00u;
    reconnect_init(&r, near_wrap);
    reconnect_failed(&r, near_wrap);

    uint32_t after_wrap = near_wrap + reconnect_delay_ms(&r); /* wraps */
    CHECK(reconnect_should_try(&r, after_wrap));
}

static void test_guards(void)
{
    reconnect_init(0, 0u);
    CHECK(!reconnect_should_try(0, 0u));
    CHECK(!reconnect_sink_busy(0, 0u));
    reconnect_failed(0, 0u);
    reconnect_succeeded(0);
    CHECK_EQ(reconnect_delay_ms(0), RECONNECT_FIRST_DELAY_MS);
}

TEST_MAIN_BEGIN()

RUN(test_accepts_real_sinks);
RUN(test_rejects_everything_else);
RUN(test_range_gate);
RUN(test_nearest_sink_wins);
RUN(test_nothing_in_range);
RUN(test_selection_is_stable);
RUN(test_backoff_grows_and_caps);
RUN(test_waits_between_attempts);
RUN(test_success_stops_retrying);
RUN(test_sink_busy_is_reported_honestly);
RUN(test_survives_clock_wrap);
RUN(test_guards);

TEST_MAIN_END()

/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the portable core. The byte vectors are written out by hand from the air format in
 * README.md. Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_core.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static const uint8_t HOST[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
static const uint8_t FM1[6] = {0x8C, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
static const uint8_t FM2[6] = {0x8C, 0xAA, 0xBB, 0xCC, 0xDD, 0xEF};
static const uint8_t LINK_MC[6] = {0x01, 0x00, 0x5E, 0x4C, 0x4E, 0x4B};
static const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/* a UDP multicast to 224.76.78.75:20808 from 169.254.1.2, 4 payload bytes "LINK" */
static const uint8_t ETH_LINK[] = {
    0x01, 0x00, 0x5E, 0x4C, 0x4E, 0x4B, /* dst */
    0x02, 0x11, 0x22, 0x33, 0x44, 0x55, /* src */
    0x08, 0x00,
    0x45, 0x00, 0x00, 0x20, 0x00, 0x01, 0x40, 0x00, 0x01, 0x11, 0x00, 0x00, /* IPv4, TTL 1, UDP */
    169, 254, 1, 2, 224, 76, 78, 75,
    0x51, 0x48, 0x51, 0x48, 0x00, 0x0C, 0x00, 0x00, /* ports 20808 */
    'L', 'I', 'N', 'K'
};
/* the same on the air, GROUP 3, sequence number 0 */
static const uint8_t AIR_LINK[] = {
    0x08, 0x00, 0x00, 0x00,             /* data, ToDS = FromDS = 0, duration 0 */
    0x01, 0x00, 0x5E, 0x4C, 0x4E, 0x4B, /* addr1 destination */
    0x02, 0x11, 0x22, 0x33, 0x44, 0x55, /* addr2 sender */
    0x02, 0x66, 0x6D, 0x31, 0x6A, 0x03, /* addr3 the group's BSSID */
    0x00, 0x00,                         /* sequence */
    0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00,
    0x45, 0x00, 0x00, 0x20, 0x00, 0x01, 0x40, 0x00, 0x01, 0x11, 0x00, 0x00,
    169, 254, 1, 2, 224, 76, 78, 75,
    0x51, 0x48, 0x51, 0x48, 0x00, 0x0C, 0x00, 0x00,
    'L', 'I', 'N', 'K'
};

static uint8_t ebuf[JK_ETH_MAX + 64], abuf[JK_AIR_MAX + 64], tmp[2048];

static void bridge(struct jk_bridge *b, uint32_t filter)
{
    CHECK(jk_init(b, HOST, 3, filter) == 0);
}

/* an Ethernet frame from the computer */
static size_t eth(const uint8_t dst[6], const uint8_t src[6], uint16_t type, const uint8_t *pl, size_t n)
{
    memcpy(tmp, dst, 6);
    memcpy(tmp + 6, src, 6);
    tmp[12] = (uint8_t)(type >> 8);
    tmp[13] = (uint8_t)type;
    if (pl)
        memcpy(tmp + 14, pl, n);
    else
        memset(tmp + 14, 0, n);
    return 14u + n;
}

/* an air frame from a unit of GROUP 3 */
static size_t air(const uint8_t da[6], const uint8_t sa[6], uint16_t seq, uint8_t fc1, uint16_t type, const uint8_t *pl, size_t n)
{
    static const uint8_t bssid[6] = {0x02, 0x66, 0x6D, 0x31, 0x6A, 0x03};
    uint16_t sc = (uint16_t)(seq << 4);
    memset(tmp, 0, 24);
    tmp[0] = 0x08;
    tmp[1] = fc1;
    memcpy(tmp + 4, da, 6);
    memcpy(tmp + 10, sa, 6);
    memcpy(tmp + 16, bssid, 6);
    tmp[22] = (uint8_t)sc;
    tmp[23] = (uint8_t)(sc >> 8);
    memcpy(tmp + 24, "\xAA\xAA\x03\x00\x00\x00", 6);
    tmp[30] = (uint8_t)(type >> 8);
    tmp[31] = (uint8_t)type;
    if (pl)
        memcpy(tmp + 32, pl, n);
    else
        memset(tmp + 32, 0, n);
    return 32u + n;
}

static void test_group_table(void)
{
    static const uint8_t ch[10] = {0, 1, 6, 11, 1, 6, 11, 1, 6, 11};
    uint8_t b[6];
    for (uint32_t g = 1; g <= 9; g++) {
        CHECK(jk_channel(g) == ch[g]);
        CHECK(jk_bssid(g, b) == 0);
        CHECK(!memcmp(b, "\x02\x66\x6D\x31\x6A", 5) && b[5] == g);
    }
    CHECK(jk_channel(0) == 0 && jk_channel(10) == 0);
    CHECK(jk_bssid(0, b) == -1 && jk_bssid(10, b) == -1);
}

static void test_init_rejects(void)
{
    struct jk_bridge b;
    CHECK(jk_init(&b, HOST, 0, JK_FILTER_LINK) == -1);
    CHECK(jk_init(&b, HOST, 10, JK_FILTER_LINK) == -1);
    CHECK(jk_init(&b, HOST, 1, 2) == -1);
    CHECK(jk_init(&b, BCAST, 1, JK_FILTER_LINK) == -1);
    CHECK(jk_init(&b, HOST, 9, JK_FILTER_ALL) == 0);
    CHECK(b.channel == 11 && b.group == 9 && b.bssid[5] == 9);
}

static void test_mac_from_efuse(void)
{
    uint8_t m[6];
    jk_mac_from_efuse((const uint8_t *)"\x7D\x11\x22\x33\x44\x55", m);
    CHECK(!memcmp(m, "\x02\x6A\x6B\x33\x44\x55", 6));   /* jank's prefix, the chip's last three bytes */
    CHECK(jk_mac_is_jank(m));
    CHECK(!jk_mac_is_jank((const uint8_t *)"\x8C\x6A\x6B\x33\x44\x55"));
}

static void test_tx_vector(void)
{
    struct jk_bridge b;
    bridge(&b, JK_FILTER_LINK);
    size_t n = jk_eth_to_air(&b, ETH_LINK, sizeof ETH_LINK, 0, abuf, sizeof abuf);
    CHECK(n == sizeof AIR_LINK);
    CHECK(!memcmp(abuf, AIR_LINK, sizeof AIR_LINK));
    CHECK(b.st.usb_in == 1 && b.st.air_out == 1);
}

static void test_tx_sequence(void)
{
    struct jk_bridge b;
    bridge(&b, JK_FILTER_LINK);
    b.seq = 0x0ABC;
    CHECK(jk_eth_to_air(&b, ETH_LINK, sizeof ETH_LINK, 0, abuf, sizeof abuf));
    CHECK(abuf[22] == 0xC0 && abuf[23] == 0xAB);    /* fragment 0, 0xABC << 4, little endian */
    b.seq = 0x0FFF;
    CHECK(jk_eth_to_air(&b, ETH_LINK, sizeof ETH_LINK, 0, abuf, sizeof abuf));
    CHECK(abuf[22] == 0xF0 && abuf[23] == 0xFF);
    CHECK(jk_eth_to_air(&b, ETH_LINK, sizeof ETH_LINK, 0, abuf, sizeof abuf));
    CHECK(abuf[22] == 0x00 && abuf[23] == 0x00);    /* 12 bits wrap */
}

static void test_tx_unicast(void)
{
    struct jk_bridge b;
    uint8_t ip[28] = {0x45};
    bridge(&b, JK_FILTER_LINK);
    ip[9] = 17;
    size_t l = eth(FM1, HOST, 0x0800, ip, sizeof ip);
    size_t n = jk_eth_to_air(&b, tmp, l, 0, abuf, sizeof abuf);
    CHECK(n == 24 + 8 + 28);
    CHECK(!memcmp(abuf + 4, FM1, 6) && !memcmp(abuf + 10, HOST, 6));
}

static void test_filter_link(void)
{
    struct jk_bridge b;
    uint8_t arp[28] = {0}, v4[28] = {0x45};
    bridge(&b, JK_FILTER_LINK);
    /* ARP who-has, broadcast: passes */
    CHECK(jk_eth_to_air(&b, tmp, eth(BCAST, HOST, 0x0806, arp, 28), 0, abuf, sizeof abuf) != 0);
    /* IPv6 multicast: dropped */
    static const uint8_t v6mc[6] = {0x33, 0x33, 0, 0, 0, 0xFB};
    CHECK(jk_eth_to_air(&b, tmp, eth(v6mc, HOST, 0x86DD, NULL, 40), 0, abuf, sizeof abuf) == 0);
    /* IPv6 unicast: dropped */
    CHECK(jk_eth_to_air(&b, tmp, eth(FM1, HOST, 0x86DD, NULL, 40), 0, abuf, sizeof abuf) == 0);
    /* IPv4 broadcast, DHCP discover: dropped */
    v4[9] = 17;
    CHECK(jk_eth_to_air(&b, tmp, eth(BCAST, HOST, 0x0800, v4, 28), 0, abuf, sizeof abuf) == 0);
    /* mDNS, another multicast group: dropped */
    static const uint8_t mdns[6] = {0x01, 0x00, 0x5E, 0x00, 0x00, 0xFB};
    CHECK(jk_eth_to_air(&b, tmp, eth(mdns, HOST, 0x0800, v4, 28), 0, abuf, sizeof abuf) == 0);
    /* an IGMP report for Link's group: dropped */
    v4[9] = 2;
    CHECK(jk_eth_to_air(&b, tmp, eth(LINK_MC, HOST, 0x0800, v4, 28), 0, abuf, sizeof abuf) == 0);
    /* Link's MAC, UDP, another port: dropped */
    v4[9] = 17;
    v4[22] = 0x12;
    v4[23] = 0x34;
    CHECK(jk_eth_to_air(&b, tmp, eth(LINK_MC, HOST, 0x0800, v4, 28), 0, abuf, sizeof abuf) == 0);
    /* Link's MAC, UDP, port 20808: passes */
    v4[22] = 0x51;
    v4[23] = 0x48;
    CHECK(jk_eth_to_air(&b, tmp, eth(LINK_MC, HOST, 0x0800, v4, 28), 0, abuf, sizeof abuf) != 0);
    /* with IP options the port is further on */
    uint8_t v4o[32] = {0x46};
    v4o[9] = 17;
    v4o[26] = 0x51;
    v4o[27] = 0x48;
    CHECK(jk_eth_to_air(&b, tmp, eth(LINK_MC, HOST, 0x0800, v4o, 32), 0, abuf, sizeof abuf) != 0);
    /* truncated IPv4 to the Link MAC: dropped, never read past the end */
    CHECK(jk_eth_to_air(&b, tmp, eth(LINK_MC, HOST, 0x0800, v4, 10), 0, abuf, sizeof abuf) == 0);
    CHECK(b.st.drop_filter == 7);
}

static void test_filter_all(void)
{
    struct jk_bridge b;
    bridge(&b, JK_FILTER_ALL);
    CHECK(jk_eth_to_air(&b, tmp, eth(FM1, HOST, 0x86DD, NULL, 40), 0, abuf, sizeof abuf) == 24 + 8 + 40);
    CHECK(b.st.drop_filter == 0);
}

static void test_tx_bad(void)
{
    struct jk_bridge b;
    bridge(&b, JK_FILTER_ALL);
    CHECK(jk_eth_to_air(&b, ETH_LINK, 13, 0, abuf, sizeof abuf) == 0);     /* short */
    CHECK(b.st.drop_bad == 1);
    CHECK(jk_eth_to_air(&b, ETH_LINK, 14, 0, abuf, sizeof abuf) != 0);     /* empty payload is still a frame */
    /* a length field instead of an EtherType: not bridgeable with SNAP */
    CHECK(jk_eth_to_air(&b, tmp, eth(FM1, HOST, 0x0040, NULL, 64), 0, abuf, sizeof abuf) == 0);
    /* 1468 fits, 1469 does not */
    CHECK(jk_eth_to_air(&b, tmp, eth(FM1, HOST, 0x0800, NULL, 1468), 0, abuf, sizeof abuf) == 1500);
    CHECK(jk_eth_to_air(&b, tmp, eth(FM1, HOST, 0x0800, NULL, 1469), 0, abuf, sizeof abuf) == 0);
    CHECK(b.st.drop_big == 1);
    /* a caller's short buffer is never overrun */
    size_t l = eth(FM1, HOST, 0x0800, NULL, 100);
    CHECK(jk_eth_to_air(&b, tmp, l, 0, abuf, 100) == 0);
    /* a group address as source: dropped */
    CHECK(jk_eth_to_air(&b, tmp, eth(FM1, BCAST, 0x0800, NULL, 40), 0, abuf, sizeof abuf) == 0);
    CHECK(b.st.mac_adopted == 0);
}

static void test_mac_adoption(void)
{
    struct jk_bridge b;
    static const uint8_t OTHER[6] = {0x06, 1, 2, 3, 4, 5}, THIRD[6] = {0x06, 9, 9, 9, 9, 9};
    uint8_t arp[28] = {0};
    bridge(&b, JK_FILTER_LINK);
    /* filtered chatter does not move the address */
    CHECK(jk_eth_to_air(&b, tmp, eth(FM1, OTHER, 0x86DD, NULL, 40), 100, abuf, sizeof abuf) == 0);
    CHECK(!b.mac_pending && !memcmp(b.mac, HOST, 6));
    /* the computer really uses another address: it goes out as sent, the radio is told to follow */
    CHECK(jk_eth_to_air(&b, tmp, eth(BCAST, OTHER, 0x0806, arp, 28), 100, abuf, sizeof abuf) != 0);
    CHECK(!memcmp(abuf + 10, OTHER, 6) && !memcmp(b.mac, OTHER, 6));
    CHECK(b.mac_pending == 1 && b.st.mac_adopted == 1);
    /* a third within the hold: dropped, not flapping */
    CHECK(jk_eth_to_air(&b, tmp, eth(BCAST, THIRD, 0x0806, arp, 28), 100 + JK_ADOPT_HOLD_MS - 1, abuf, sizeof abuf) == 0);
    CHECK(b.st.drop_foreign == 1 && !memcmp(b.mac, OTHER, 6));
    /* after it, adopted */
    CHECK(jk_eth_to_air(&b, tmp, eth(BCAST, THIRD, 0x0806, arp, 28), 100 + JK_ADOPT_HOLD_MS, abuf, sizeof abuf) != 0);
    CHECK(!memcmp(b.mac, THIRD, 6) && b.st.mac_adopted == 2);
}

static void test_rx_vector(void)
{
    struct jk_bridge b;
    uint8_t a[sizeof AIR_LINK];
    bridge(&b, JK_FILTER_LINK);
    memcpy(a, AIR_LINK, sizeof a);
    memcpy(a + 10, FM1, 6);
    size_t n = jk_air_to_eth(&b, a, sizeof a, -44, 1000, ebuf, sizeof ebuf);
    CHECK(n == sizeof ETH_LINK);
    CHECK(!memcmp(ebuf, LINK_MC, 6) && !memcmp(ebuf + 6, FM1, 6));
    CHECK(!memcmp(ebuf + 12, ETH_LINK + 12, sizeof ETH_LINK - 12));
    CHECK(b.st.air_in == 1 && b.st.usb_out == 1);
    CHECK(jk_peers_heard(&b, 1000) == 1 && jk_best_rssi(&b, 1000) == -44);
}

static void test_rx_unicast(void)
{
    struct jk_bridge b;
    bridge(&b, JK_FILTER_LINK);
    /* to the computer: passed */
    CHECK(jk_air_to_eth(&b, tmp, air(HOST, FM1, 1, 0, 0x0800, NULL, 40), -50, 0, ebuf, sizeof ebuf) == 54);
    /* to another unit: not passed, but the sender is counted */
    CHECK(jk_air_to_eth(&b, tmp, air(FM2, FM1, 2, 0, 0x0800, NULL, 40), -50, 0, ebuf, sizeof ebuf) == 0);
    CHECK(b.st.drop_other == 1);
    /* from a unit not heard before, to another: it is a unit of the group all the same */
    CHECK(jk_air_to_eth(&b, tmp, air(FM1, FM2, 2, 0, 0x0800, NULL, 40), -60, 0, ebuf, sizeof ebuf) == 0);
    CHECK(jk_peers_heard(&b, 0) == 2);
    /* broadcast ARP: passed */
    CHECK(jk_air_to_eth(&b, tmp, air(BCAST, FM1, 3, 0, 0x0806, NULL, 28), -50, 0, ebuf, sizeof ebuf) == 42);
}

static void test_rx_rejects(void)
{
    struct jk_bridge b;
    bridge(&b, JK_FILTER_LINK);
    size_t l;
    /* our own frame back */
    CHECK(jk_air_to_eth(&b, tmp, air(LINK_MC, HOST, 1, 0, 0x0800, NULL, 40), -50, 0, ebuf, sizeof ebuf) == 0);
    CHECK(b.st.drop_own == 1);
    /* another group: not even counted as heard */
    l = air(LINK_MC, FM1, 1, 0, 0x0800, NULL, 40);
    tmp[21] = 4;
    CHECK(jk_air_to_eth(&b, tmp, l, -50, 0, ebuf, sizeof ebuf) == 0);
    CHECK(jk_peers_heard(&b, 0) == 0);
    /* ToDS, FromDS, Protected */
    CHECK(jk_air_to_eth(&b, tmp, air(LINK_MC, FM1, 1, 0x01, 0x0800, NULL, 40), -50, 0, ebuf, sizeof ebuf) == 0);
    CHECK(jk_air_to_eth(&b, tmp, air(LINK_MC, FM1, 1, 0x02, 0x0800, NULL, 40), -50, 0, ebuf, sizeof ebuf) == 0);
    CHECK(jk_air_to_eth(&b, tmp, air(LINK_MC, FM1, 1, 0x40, 0x0800, NULL, 40), -50, 0, ebuf, sizeof ebuf) == 0);
    /* a group address as sender */
    CHECK(jk_air_to_eth(&b, tmp, air(LINK_MC, BCAST, 1, 0, 0x0800, NULL, 40), -50, 0, ebuf, sizeof ebuf) == 0);
    /* a management frame */
    l = air(LINK_MC, FM1, 1, 0, 0x0800, NULL, 40);
    tmp[0] = 0x80;
    CHECK(jk_air_to_eth(&b, tmp, l, -50, 0, ebuf, sizeof ebuf) == 0);
    /* null data: no payload */
    l = air(LINK_MC, FM1, 1, 0, 0x0800, NULL, 40);
    tmp[0] = 0x48;
    CHECK(jk_air_to_eth(&b, tmp, l, -50, 0, ebuf, sizeof ebuf) == 0);
    /* protocol version not 0 */
    l = air(LINK_MC, FM1, 1, 0, 0x0800, NULL, 40);
    tmp[0] = 0x09;
    CHECK(jk_air_to_eth(&b, tmp, l, -50, 0, ebuf, sizeof ebuf) == 0);
    /* no LLC/SNAP */
    l = air(LINK_MC, FM1, 1, 0, 0x0800, NULL, 40);
    tmp[24] = 0;
    CHECK(jk_air_to_eth(&b, tmp, l, -50, 0, ebuf, sizeof ebuf) == 0);
    /* a fragment */
    l = air(LINK_MC, FM1, 1, 0, 0x0800, NULL, 40);
    tmp[22] |= 1;
    CHECK(jk_air_to_eth(&b, tmp, l, -50, 0, ebuf, sizeof ebuf) == 0);
    CHECK(jk_air_to_eth(&b, tmp, air(LINK_MC, FM1, 1, 0x04, 0x0800, NULL, 40), -50, 0, ebuf, sizeof ebuf) == 0);
    /* every shorter length of a good frame: nothing read out of bounds, nothing passed under the headers */
    l = air(LINK_MC, FM1, 9, 0, 0x0800, NULL, 0);
    for (size_t i = 0; i < l; i++)
        CHECK(jk_air_to_eth(&b, tmp, i, -50, 0, ebuf, sizeof ebuf) == 0);
    /* a caller's short buffer */
    l = air(LINK_MC, FM1, 2, 0, 0x0800, NULL, 40);
    CHECK(jk_air_to_eth(&b, tmp, l, -50, 0, ebuf, 20) == 0);
}

static void test_rx_qos(void)
{
    struct jk_bridge b;
    bridge(&b, JK_FILTER_LINK);
    /* a computer's QoS data: 2 more header bytes before the LLC */
    size_t l = air(LINK_MC, FM1, 1, 0, 0x0800, (const uint8_t *)"abcdefgh", 8);
    memmove(tmp + 26, tmp + 24, l - 24);
    tmp[24] = tmp[25] = 0;
    tmp[0] = 0x88;
    size_t n = jk_air_to_eth(&b, tmp, l + 2, -50, 0, ebuf, sizeof ebuf);
    CHECK(n == 14 + 8 && !memcmp(ebuf + 14, "abcdefgh", 8));
    /* A-MSDU: not unpacked here */
    l = air(LINK_MC, FM1, 2, 0, 0x0800, (const uint8_t *)"abcdefgh", 8);
    memmove(tmp + 26, tmp + 24, l - 24);
    tmp[24] = 0x80;
    tmp[25] = 0;
    tmp[0] = 0x88;
    CHECK(jk_air_to_eth(&b, tmp, l + 2, -50, 0, ebuf, sizeof ebuf) == 0);
}

static void test_rx_dup(void)
{
    struct jk_bridge b;
    bridge(&b, JK_FILTER_LINK);
    CHECK(jk_air_to_eth(&b, tmp, air(HOST, FM1, 5, 0, 0x0800, NULL, 40), -50, 0, ebuf, sizeof ebuf) != 0);
    /* the retry of it: same number, Retry bit */
    CHECK(jk_air_to_eth(&b, tmp, air(HOST, FM1, 5, 0x08, 0x0800, NULL, 40), -50, 0, ebuf, sizeof ebuf) == 0);
    CHECK(b.st.drop_dup == 1);
    /* a new frame with the Retry bit, its first copy was lost */
    CHECK(jk_air_to_eth(&b, tmp, air(HOST, FM1, 6, 0x08, 0x0800, NULL, 40), -50, 0, ebuf, sizeof ebuf) != 0);
    /* the same number without the Retry bit: a unit that restarted, passed */
    CHECK(jk_air_to_eth(&b, tmp, air(HOST, FM1, 6, 0, 0x0800, NULL, 40), -50, 0, ebuf, sizeof ebuf) != 0);
    /* numbers are per sender */
    CHECK(jk_air_to_eth(&b, tmp, air(HOST, FM2, 6, 0x08, 0x0800, NULL, 40), -50, 0, ebuf, sizeof ebuf) != 0);
}

static void test_peers(void)
{
    struct jk_bridge b;
    uint8_t m[6] = {0x8C, 0, 0, 0, 0, 0};
    bridge(&b, JK_FILTER_LINK);
    for (uint32_t i = 0; i < JK_PEERS + 2; i++) {       /* a full table still bridges */
        m[5] = (uint8_t)i;
        CHECK(jk_air_to_eth(&b, tmp, air(LINK_MC, m, 1, 0, 0x0800, NULL, 40), (int8_t)(-40 - (int)i), 100, ebuf, sizeof ebuf) != 0);
    }
    CHECK(jk_peers_heard(&b, 100) == JK_PEERS);
    CHECK(jk_best_rssi(&b, 100) == -40);
    /* just before and at the time limit */
    CHECK(jk_peers_heard(&b, 100 + JK_HEARD_MS - 1) == JK_PEERS);
    CHECK(jk_peers_heard(&b, 100 + JK_HEARD_MS) == 0);
    CHECK(jk_best_rssi(&b, 100 + JK_HEARD_MS) == 0);
    /* a gone unit's place is reused */
    m[5] = 0xAA;
    CHECK(jk_air_to_eth(&b, tmp, air(LINK_MC, m, 1, 0, 0x0800, NULL, 40), -70, 5000, ebuf, sizeof ebuf) != 0);
    CHECK(jk_peers_heard(&b, 5000) == 1);
    /* the ms counter wraps */
    bridge(&b, JK_FILTER_LINK);
    CHECK(jk_air_to_eth(&b, tmp, air(LINK_MC, FM1, 1, 0, 0x0800, NULL, 40), -70, 0xFFFFFF00u, ebuf, sizeof ebuf) != 0);
    CHECK(jk_peers_heard(&b, 0x10) == 1);
}

static void test_set_group(void)
{
    struct jk_bridge b;
    bridge(&b, JK_FILTER_LINK);
    CHECK(jk_air_to_eth(&b, tmp, air(LINK_MC, FM1, 1, 0, 0x0800, NULL, 40), -50, 0, ebuf, sizeof ebuf) != 0);
    CHECK(jk_set_group(&b, 0) == -1 && b.group == 3);
    CHECK(jk_set_group(&b, 5) == 0);
    CHECK(b.group == 5 && b.channel == 6 && b.bssid[5] == 5);
    CHECK(jk_peers_heard(&b, 0) == 0);
    CHECK(b.st.air_in == 1);          /* (counters stay) */
}

static void test_is_group(void)
{
    struct jk_bridge b;
    bridge(&b, JK_FILTER_LINK);
    CHECK(jk_air_is_group(b.bssid, AIR_LINK, sizeof AIR_LINK) == 1);
    CHECK(jk_air_is_group(b.bssid, AIR_LINK, 23) == 0);
    uint8_t a[sizeof AIR_LINK];
    memcpy(a, AIR_LINK, sizeof a);
    a[21] = 2;
    CHECK(jk_air_is_group(b.bssid, a, sizeof a) == 0);
    memcpy(a, AIR_LINK, sizeof a);
    a[0] = 0x80;
    CHECK(jk_air_is_group(b.bssid, a, sizeof a) == 0);
}

static void test_roundtrip_any_size(void)
{
    struct jk_bridge tx, rx;
    uint8_t pl[1468];
    CHECK(jk_init(&tx, HOST, 3, JK_FILTER_ALL) == 0);
    CHECK(jk_init(&rx, FM1, 3, JK_FILTER_ALL) == 0);
    for (size_t i = 0; i < sizeof pl; i++)
        pl[i] = (uint8_t)(i * 7u);
    for (size_t n = 0; n <= sizeof pl; n += 67) {
        size_t el = eth(FM1, HOST, 0x0800, pl, n);
        size_t al = jk_eth_to_air(&tx, tmp, el, 0, abuf, sizeof abuf);
        CHECK(al == 32 + n);
        CHECK(jk_air_to_eth(&rx, abuf, al, -40, 0, ebuf, sizeof ebuf) == el);
        CHECK(!memcmp(ebuf, tmp, el));
    }
}

int main(void)
{
    test_group_table();
    test_init_rejects();
    test_mac_from_efuse();
    test_tx_vector();
    test_tx_sequence();
    test_tx_unicast();
    test_filter_link();
    test_filter_all();
    test_tx_bad();
    test_mac_adoption();
    test_rx_vector();
    test_rx_unicast();
    test_rx_rejects();
    test_rx_qos();
    test_rx_dup();
    test_peers();
    test_set_group();
    test_is_group();
    test_roundtrip_any_size();
    printf("core: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}

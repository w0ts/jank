/* SPDX-License-Identifier: GPL-3.0-only */
/* The beat tracker as the Link session's master: its tempo and beat grid, and its stop and start, go into the session.
 * No ESP-IDF, no tracker (core/beat/ is fed by the caller), no clock: times are the Link host's microseconds.
 *
 *   tempo       set when it differs from the session's by more than 0.02 BPM
 *   phase       the session's beat grid is moved only when it is more than 5 ms from the tracked beat
 *   transport   stopped -> Link stop at once; running again -> Link start on the next bar (quantum beats), sent
 *               JST_LEAD_US before it (jank_start.h); no sound on the source (LINK mode) -> the session left as it is;
 *               the user's Stop with FOLLOW+LEAD (held) -> Link stop at the press, and no start until the Start
 *
 * Called with the Link node's owner holding its lock, every few hundred ms and on each tracked beat. */
#ifndef JANK_BEATLINK_H
#define JANK_BEATLINK_H
#include <stdint.h>
#include "link.h"
#include "jank_start.h"

#define JBL_TEMPO_DEADBAND_X1000 20     /* 0.02 BPM */
#define JBL_PHASE_US 5000

struct jbl {
    uint8_t playing;                    /* we asked the session to play and have not asked it to stop */
    uint8_t sess[8];                    /* the session `playing` is about: another one (a join) takes the tracker's start
                                         * again (a session of its own joining an older, stopped one would wait forever) */
    uint32_t tempo_sets, phase_forces, starts, stops, releases;
    int32_t err_us;                     /* the last grid error seen against the tracked beat, before any correction */
    uint32_t bar_seq;                   /* the downbeat estimator's last decision acted on */
    uint8_t bar_seen;                   /* bar_seq holds one */
    uint32_t bar_moves;                 /* whole-beat moves of the session's bar */
    uint8_t held;                       /* the last step was held (jbl_in.held): the press is acted on once */
    int8_t nudge;                       /* PHASE: the user's NUDGE BEATs in all, in half beats of Link (0: AUTO) */
    uint8_t nudge_8ths;                 /* ... and the offset the phase follower keeps, eighths of a TRACKED beat, mod 32 */
    uint32_t nudges;                    /* nudges taken */
};

/* the downbeat estimator (core/beat/jb_down.h) as the bar step wants it */
struct jbl_bar {
    uint8_t locked;                     /* it knows the bar's 1 */
    uint32_t seq;                       /* its decision (one a counted beat) */
    int32_t count;                      /* the count of that beat ... */
    int8_t phi;                         /* ... and the count position (mod 4) of the 1 */
    int64_t beat_us;                    /* that beat, Link host time (as jbl_in's beat_us) */
};

struct jbl_in {
    uint8_t running;                    /* the tracker follows a steady or moving beat (not stopped, audio flowing) */
    int64_t beat_us;                    /* a tracked beat, Link host time: any beat of the grid */
    double period_us;                   /* the beat grid's period */
    uint8_t passive;                    /* no sound on the source (LINK mode): the session is not the tracker's to start or stop */
    uint8_t held;                       /* AUDIO mode's Stop with LINK TRANSPORT = FOLLOW+LEAD (jank_hold.h): the session is
                                         * stopped once, at the press, then neither started nor stopped by the tracker */
    uint8_t bar_mode;                   /* AUTO BAR: enum jbl_bar_mode */
    uint8_t bar_now;                    /* AT START: a new start for the bar now (a resync, a fold change, a new lock): one
                                         * placement allowed at this step even though the session plays */
    uint8_t bar_wait;                   /* AT START: the estimator has not found the 1 yet: the tracker's start waits */
};

/* AUTO BAR (the setting): OFF no placement; AT START the bar's 1 is placed only while the session is stopped or its start
 * is not reached yet (an armed start re-targets onto the moved grid: it lands on the 1), or at a step with bar_now; never
 * while it plays. ALWAYS: every new decision, playing or not. START WAIT: with AT START the tracker's start waits for the
 * estimator (bar_wait, which the caller holds for at most JBL_BAR_WAIT_BEATS tracked beats after the lock): found in time,
 * the start lands on the music's 1; not, it starts on the session's next bar line and the 1 waits for the next start. A
 * wait bounded so: "start on beat one" is honest when the estimator can say where it is, and a start is never held up
 * for long when it cannot (a break, music with no clear bar). */
enum jbl_bar_mode { JBL_BAR_ALWAYS = 0, JBL_BAR_START = 1, JBL_BAR_OFF = 2 };   /* (0: the behaviour before the setting) */
#define JBL_BAR_WAIT_BEATS 8u

void jbl_init(struct jbl *c);
/* settled: the node is in a session (with peers, or AUDIO mode's own alone) and has its state (jp_settled). start: the
 * peer's start, sent by its owner's poll (jst_poll). now: host time. quantum: beats a bar. */
void jbl_step(struct jbl *c, struct lk_node *k, struct jst *start, int settled, int64_t now, const struct jbl_in *in, uint32_t quantum);
/* Where the session's nearest beat is from an instant, in us (+: the session's beat comes later), given the session's
 * beat position at that instant (micro-beats, lk_beat_at) and its microseconds a beat. For the beat reports: what Live
 * follows against the beat as the stick takes it to be heard. */
int32_t jbl_session_offset_us(int64_t ub_at, int32_t mpb);

/* AUTO BAR. The whole-beat move that puts the session's bar on the 1: the session's beat at the counted beat's moment
 * (ub_at, micro-beats; half a beat off after a half-beat nudge counts as the same beat) should sit at bar position
 * (count - phi) mod 4. Returns n in -1..2. */
int jbl_bar_shift(int64_t ub_at, int32_t count, int phi);
/* the session's beats jump n whole beats at t (a force; the tempo untouched); an armed start re-targets the next bar
 * line of the moved grid (jank_start.h). Called under the Link node owner's lock. */
void jbl_shift_bar(struct jbl *c, struct lk_node *k, struct jst *start, int n, int64_t t);
/* after jbl_step, with the same lock: at each new LOCKED decision, the bar onto the 1 if it is not there. Only while
 * the tracker leads (settled, not passive, running) and the phase is within JBL_PHASE_US (then the session's beat at
 * the counted beat is unambiguous; otherwise the decision waits for the next step). The user's PHASE (the nudges' total)
 * rides on top: the 1 is placed with the nudges' whole beats kept (M2h). Returns the move made, 0 none. */
int jbl_bar_step(struct jbl *c, struct lk_node *k, struct jst *start, int settled, int64_t now, const struct jbl_in *in,
                 const struct jbl_bar *bar);

/* NUDGE BEAT (the device's NO + UP/DOWN, the control line's `nudge beat`; beat-listener's controller.py shift_beats):
 * Link's beats jump `halves` half beats now (+: the session's beats come earlier, UP), and the phase follower keeps them
 * that far from the tracked grid from then on: whole beats are in the session's count, the half part is the follower's
 * offset. The user's nudges add up, clamped to +-JBL_NUDGE_MAX half beats (+-3 beats); one past it is refused whole.
 *
 * Folded (jank_fold.h), the half beat is half a beat of Link's (the folded grid's). The offset is kept in TRACKED time
 * (eighths of a tracked beat), so it means the same music across a later fold change: an off-beat fixed at fold 1 (half a
 * tracked beat) is a whole beat at x2 (both grids land on the music's beats) and stays on the on-beats at /2.
 *
 * The force needs the session's state (settled, as every force); not settled, the offset is kept and the follower puts
 * the session there once it leads. An armed start re-targets the moved grid's next bar line (jst_retarget). In LINK mode
 * (passive) the session is not the tracker's: refused. Returns 0, or -1 refused (nothing changed).
 *
 * TWO LAYERS (M2h, the user: "beat is more phase. So zero is auto"): the total is the user's PHASE, an offset on top of the
 * tracker's grid and AUTO BAR's bar. AUTO BAR keeps placing the music's 1 with the offset kept (jbl_bar_step; a nudge no
 * longer makes it MANUAL), and nothing automatic changes the PHASE (a new stream, the tracker stopped: kept). Back to a
 * total of 0: AUTO, no offset left (nudge_8ths 0, whatever folds the nudges were made at). */
#define JBL_NUDGE_MAX 6
int jbl_nudge(struct jbl *c, struct lk_node *k, struct jst *start, int settled, const struct jbl_in *in, int halves,
              int fold_exp, int64_t now);
/* half a beat of Link's in eighths of a tracked beat at fold 2^fold_exp (exp -2..2): 16 8 4 2 1 */
int jbl_half_8ths(int fold_exp);
/* where the follower puts the session's beats: this long before the (folded) tracked grid, 0 .. one Link beat. The
 * caller moves the grid's beat (jbl_in.beat_us) back by it. tracked_period_us: the tracker's own (unfolded) period */
int64_t jbl_nudge_us(const struct jbl *c, double tracked_period_us, int fold_exp);
/* no PHASE any more, the session left where it is (the tests' reset; nothing automatic calls it since M2h) */
void jbl_nudge_clear(struct jbl *c);
#endif

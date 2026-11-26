/* SPDX-License-Identifier: GPL-3.0-only */
/* Jank's Launchpad core as the FM-1 builds it (docs/launchpad.md, "Using the core on another unit"): ONE unit, every file it links #included, with its own scale list and the
 * clips page instead of the Digitone's (no jank_lpsess.c, jank_lpsessapp.c, jank_lpscales.c). tests/run.sh copies those files and tests/lpunit/'s scales into one folder, compiles
 * this freestanding (no C library headers: the FM-1's toolchain has none), and tests/test_lpunit.c includes it to test the behaviour. */
#include "jank_lpx.c"
#include "jank_lpnote.c"
#include "jank_lpscales.c"
#include "jank_lpstep.c"
#include "jank_lpmix.c"
#include "jank_lptrans.c"
#include "jank_lpapp.c"
#include "jank_lpclips.c"
#include "jank_lpprefs.c"

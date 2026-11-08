# Link peer (copied from fm1-base)

`link.c` and `link.h` are Optimist's C port of Ableton Link's tempo, beat and start/stop part, as they are in Optimist's
fm1-base (`link/`, github.com/w0ts/fm1-base), commit d63efc8 on topic/link-measure-nearest, merged as 4ef3781 into integ/optimist-main (2026-11-08: the measuring partner is the member heard last).
GPL-3.0-only, Copyright (C) 2026 Henri David. The protocol is Ableton's (Ableton Link, Copyright 2016 Ableton AG).

They build with no FM-1 dependency. Keep them in step with fm1-base's copy: a change to the air format goes to both.

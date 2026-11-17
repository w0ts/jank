/* SPDX-License-Identifier: GPL-3.0-only */
/* madmom's online beat network (BEATS_LSTM[0]): three LSTM layers of 25 units with peephole connections, then one
 * sigmoid output, one frame at a time. The weights are float32 in beat_model.c, in flash (const): the state is 600 bytes. */
#ifndef JB_LSTM_H
#define JB_LSTM_H
#include <stddef.h>
#include "beat_model.h"

#define JB_LAYERS 3

struct jb_lstm {
    float h[JB_LAYERS][BEAT_HIDDEN];   /* each layer's last output */
    float c[JB_LAYERS][BEAT_HIDDEN];   /* each layer's cell state */
};

void jb_lstm_init(struct jb_lstm *s);
/* One frame of BEAT_INPUTS features in, the beat activation (0..1) out. */
float jb_lstm_step(struct jb_lstm *s, const float *feat);

/* The same network with its weights in RAM as int16 (jb_lstm_q16.c): 59 KB of RAM instead of 115 KB of flash read every
 * frame. Build once (it reads the float weights), then step with the same state struct. */
void jb_lstm_q16_build(void);
size_t jb_lstm_q16_ram(void);           /* bytes of RAM the int16 weights, scales and the tanh table take */
float jb_lstm_q16_step(struct jb_lstm *s, const float *feat);

#endif

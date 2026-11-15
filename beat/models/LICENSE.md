# beat_ours.pt and candidate/beat_model.[ch]

Beat-activation net for jank's C core (core/beat/): madmom's online front end (162 inputs, 100 fps), 3 unidirectional
peephole LSTM layers of 25 units, a 25 -> 1 sigmoid. Same architecture as madmom's BEATS_LSTM[0]; only the weights differ.

## Licence of the weights

Ours, randomly initialised and trained by `beat-listener/train/`. Not derived from madmom's weights (none were loaded
into training). Offered under GPL-3.0-or-later, so jank (GPL-3.0) can ship them.

Not ours, and separate from the weights:
- The log filterbank in `candidate/beat_model.c` and the feature chain it matches are madmom's code (BSD-3-Clause).
- madmom's own models are CC BY-NC-SA 4.0. They were used as a measuring instrument for the label correction (below),
  never as initial weights, teacher or training input.

## Training data

- 518 tracks (48.0 h) from the user's own collection (Traktor library, mostly electronic), their audio decoded to mono
  44.1 kHz. Validation: 85 tracks (7.4 h). Test: 113 tracks (9.8 h). Split by download folder, folders sharing a title
  kept together.
- Labels: Traktor's constant-tempo beat grids, each corrected by one constant per-track offset measured against madmom's
  offline beats (see `train/make_labels.py`; the bias of those beats against GTZAN's hand annotations, -10 ms, is taken
  off). 113 of 829 tracks were dropped (no stable offset).
- Augmentation: the training tracks also played 0.85 / 0.92 / 1.08 / 1.17 times as fast (tempo and pitch), labels scaled.
- GTZAN and the 29 DJ-set mixes were never trained on, only used as held-out tests.

## Open questions (not legal conclusions)

- The tracks are purchased or downloaded music. Whether weights trained on it can be redistributed under the GPL is
  not settled by anything here; the training audio is not distributed, but nobody has checked this with a lawyer.
- The labels use output of madmom's CC BY-NC-SA models, only as one constant per track. Whether that touches the
  weights' licence is untested.
- Traktor's grid data and the track metadata are the user's.

## Files

- `beat_ours.pt`: torch state_dict (`model`) and the step (2500), run E.
- `candidate/beat_model.c|h`: the C export (`scripts/export_beat_c.py model OUT --weights models/beat_ours.pt`); the input
  normalisation is folded into layer 0.
- `EVAL_test.txt`: the held-out numbers (ours vs madmom's net 0, same downstream). `train_E_log.txt`: training log.

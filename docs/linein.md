# LINE IN into the beat tracker

The board's line input (jank mini 2.2: the LMV321 summer into GPIO9) goes to the beat tracker the same way USB audio does.
Code: `firmware/main/linein.c`, `core/jank_line.c`, the resampler `jp_cr` in `core/jank_pcm.c`; host tests `tests/test_line.c`.
The design follows the timing study (`worktrees/timing-scratch/TIMING.md`, section 5).

## The path

| Step | Where | What |
|---|---|---|
| ADC1 continuous mode | GDMA, `adc_continuous` | pattern [LINE IN, cell] at 83,333 Hz (the S3's maximum, APB / 16 / 2 / 30): each channel every 24 us exactly. Without a cell (the XIAO bench) [LINE IN] at 41,666 Hz, the same 24 us. 12 dB attenuation |
| DMA interrupt | core 0, IRAM (`CONFIG_ADC_CONTINUOUS_ISR_IRAM_SAFE`) | once per frame of 200 line samples (4.8 ms): stamps it with the local clock (the gptimer of `midi.c`), splits line samples and the cell's sum (`jl_parse`), wakes the task. Runs while flash is written |
| Time | `jl_fit` | sample n is at t0 + 24 n us (the ADC and the timer both run from APB: no drift). t0 = the lowest stamp minus 24 n over the last ~2 s (the interrupt is only ever late) |
| Level | `jl_cond` | the 12-bit codes x 16 to int16, the DC removed (one pole, ~6.5 Hz), the gain `line_gain` (-12..+12 dB, 0 at every start), clips counted |
| 44.1 kHz | `jp_cr` | cubic (Catmull-Rom), exactly 1323 outputs per 1250 inputs, only while the LINE IN is the source |
| Into the tracker | `uac_line_feed` | the same ring and sample-time offset as USB audio, with each block's sample-stamped time: `beat`, `beat_latency` and AUTO BAR are unchanged |
| Cell | mini 2.2 only | averaged each second, millivolts by the eFuse calibration, x 2 (R10 / R11), corrected by a oneshot reading taken at the start; the HOME line's battery percent (empty 3.3 V: the TPS63802) |

Source: `set source auto|line|usb` (AUTO: the line when a plug is in, GPIO16, else USB audio when a stream plays). A change of
source restarts the tracker. Lost frames become silence, so the times stay right.

## `line` (control line)

| Key | Expected |
|---|---|
| `line_in` | `running` (`none` on a board without LINE IN; `failed` with `err`) |
| `feeds_tracker` | `yes` when the tracker hears the line |
| `adc_hz`, `interval`, `channels` | 83333 / 30 / 2 (mini 2.2), 41666 / 60 / 1 (XIAO bench) |
| `frames` | +208 a second |
| `lost`, `isr_dropped`, `short`, `other` | 0 |
| `lat_avg_us`, `lat_max_us` | the interrupt's lateness against the fit: a few us, tens at worst |
| `fit_steps`, `drift_ppb` | 0, and within +-1000 (the ADC and the timer share APB) |
| `dc_code` | the bias in ADC codes (~1800 on the board, ~2000 on the bench network) |
| `peak_db`, `rms_db`, `clips` | the last second; music peaks -12..-3, clips 0; silence: the noise floor |
| `cell_mv`, `cell_boot_mv`, `cell_pattern_mv`, `batt_pct` | mini 2.2: the cell; `batt_pct` -1 with no cell |
| `cpu_pct`, `isr_cycles`, `task_cycles` | core 0's share for this path (interrupt + task), per frame |

## Trying it on the XIAO (bench option)

The XIAO has no LINE IN. The bench build (`sdkconfig.linein-bench`, `CONFIG_JANK_LINEIN_BENCH`) samples pad **D3 = GPIO4**
(ADC1_CH3; not a strapping pin, unused by the firmware). A line output into it needs a bias network:

```
XIAO 3V3 --- 10k ---+--- 10k --- XIAO GND
                    |
line out tip -- 4.7uF (+ to the node) --+
                    |
                    +--- 1k --- D3 --- 10nF --- GND
line out sleeve -------------------------------- XIAO GND
```

- Bias 1.65 V, high-pass ~7 Hz (5 k with 4.7 uF), low-pass ~16 kHz (1 k with 10 nF; the 10 nF also feeds the ADC's
  sampling capacitor). The 1 k limits the current if the source swings past the rails.
- Level: the input clips beyond about +-1.4 V peak around the bias (~1 V RMS). Start with the interface's output low and
  raise it until loud parts show `peak_db` about -6 and `clips` stays 0.
- A balanced output: tip and sleeve only (the ring left open), or a TS cable.
- Then `set source line`, and read `line`, `beat` (`audio=1`, `packets` +208 a second, `gaps` 0, `overflows` 0, `mode`,
  `bpm`) and `home` (the level).

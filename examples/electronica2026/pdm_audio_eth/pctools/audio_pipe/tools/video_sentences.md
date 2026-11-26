# Checked sentences for the video (Scene Simulator, "Factory (booth)")

Each **set** below was typed into the Scene Simulator's **Factory (booth)**
scene (Kokoro voices), exported, looped through the LP-AM243 board and
transcribed by `audio_pipe live`. Every worker came out word for word right
on every pass, and no text appeared on any other direction
(`tools/booth_curate.py`, 8-10 passes per set, 2026-11-08).

**Use a whole set.** Sentences that pass in one set can fail in another:
the four workers overlap, and a different overlap changes what whisper
hears (e.g. "Check the oil level on the big motor." came out as "oil
number" next to other sentences, and some combinations made whisper write
"Yeah!" at 170°, where nobody talks). Type the sentences exactly as below,
punctuation included (Kokoro's speech follows it).

| Worker in the page | Voice (Kokoro) | Direction, distance | Starts at |
|---|---|---|---|
| Speaker 1, male | am_michael | 30°, 1.5 m | 0.3 s |
| Speaker 2, female | af_heart | 0°, 1.5 m | 0.7 s |
| Speaker 3, male low | bm_george | 210°, 1.8 m, -1 dB | 1.1 s |
| Speaker 4, female high | bf_emma | 270°, 2.5 m, -3 dB | 1.5 s |

## Set A (the scene's default text: nothing to type)

1. Hi there, how are you doing today?
2. The forklift is waiting at gate two.
3. Please bring the red toolbox to station four.
4. The conveyor belt stopped five minutes ago.

## Set B

1. Good morning, the night shift went well.
2. Quality control approved the first batch.
3. The welding robot needs new tips.
4. Somebody left a cart in the aisle.

## Set J (set A, speaker 4 changed)

1. Hi there, how are you doing today?
2. The forklift is waiting at gate two.
3. Please bring the red toolbox to station four.
4. Somebody left a cart in the aisle.

## Set K (set A, speaker 2 changed)

1. Hi there, how are you doing today?
2. Quality control approved the first batch.
3. Please bring the red toolbox to station four.
4. The conveyor belt stopped five minutes ago.

## Set L (set A, speakers 2-4 from set B)

1. Hi there, how are you doing today?
2. Quality control approved the first batch.
3. The welding robot needs new tips.
4. Somebody left a cart in the aisle.

## Checking a new set

Sentences of your own: export them with `tools/page_export.py` (headless
page) and loop them on the board with the transcript log, then
`booth_curate.py LOG DIR` must say PASS for the set (see
`docs/demo_howto.md` 7.8, 7.9). Of 32 proposed sentences, 22 were exact
in their first scene; of 13 complete sets tried, these 5 passed.

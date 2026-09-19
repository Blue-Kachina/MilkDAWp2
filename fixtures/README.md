# Fixtures

Audio fixtures used by `milkdawp_core` tests, the `mdw-analyze --suite`
metric gate (Phase 1.10), and engine headless render tests (Phase 2.7).

## Policy

- **Short.** Each clip is ≤10 seconds. Onset/tempo detection and transition
  scheduling only need a few bars to exercise; long clips just slow down CI.
- **Licence-clean.** Every clip is either synthesized (a script, committed
  alongside the fixture) or sourced under a licence permissive enough to
  redistribute in this repository (e.g. CC0, or original recordings the
  contributor owns outright). Record the source and licence in this file's
  fixture table when adding one.
- **Annotated.** Every music fixture ships with a matching `beats.txt`.
- **Covers the hard cases, not just the easy ones.** Phase 1.9's set spans
  four-on-the-floor, syncopated, a tempo change, a breakdown/drop, sparse
  acoustic material, silence, and noise -- because a beat tracker that only
  sees clean electronic material will look better in CI than it behaves for
  Matthew at a gig.

## Layout

```
fixtures/
├── README.md            # this file
├── thresholds.json       # pass/fail thresholds for mdw-analyze --suite (Phase 1.10)
└── <clip-name>/
    ├── audio.wav          # mono or stereo, any sample rate (mdw-analyze resamples)
    ├── beats.txt           # one beat time in seconds per line, ascending
    └── SOURCE.md            # where this clip came from and its licence
```

## `beats.txt` format

Plain text, one beat time in seconds per line, ascending, `\n`-terminated:

```
0.482
0.960
1.438
1.917
```

Downbeats are not marked separately in 1.0 -- see §4.3's downbeat heuristic
in `development_roadmap.md`; a `downbeats.txt` in the same format may be
added later without changing this schema.

## Adding a fixture

1. Add `fixtures/<name>/audio.wav` (synthesize it, or bring a licence-clean
   recording) and `fixtures/<name>/SOURCE.md` describing where it came from.
2. Annotate beats by ear or with an existing reference tracker into
   `fixtures/<name>/beats.txt`.
3. Add an entry to `fixtures/thresholds.json` once that file exists
   (Phase 1.10) so the fixture is actually checked in CI, not just present.

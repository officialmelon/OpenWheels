# OpenWheels

An open-source, mobile-first reimplementation of the Happy Wheels mobile engine
(cocos2d-x + Box2D), reconstructed from the official Android/iOS binaries with
[re-agent](https://github.com/Dryxio/reagent) (Ghidra + LLM reversal).

Goal: 1:1 behavior parity with the original mobile game so new levels and
characters can be built on a clean, licensed-clean codebase we own.

## Layout

- `binary/` — reference material only (extracted Android XAPK + iOS IPA).
  **Never redistribute.** Not part of any OpenWheels release.
- `src/` — recreated engine sources (re-agent output, human-reviewed)
- `tests/` — test oracles / differential harnesses
- `reports/re-agent/` — reversal runs, evidence, logs
- `re-agent.yaml` — reversal workflow config (provider: codex / gpt-5.6-luna)
- `ghidra-bridge.yaml` — Ghidra backend config for the analyzed binaries

## Reversal workflow

```bash
# 1. Analyze a binary in Ghidra (once per binary)
analyzeHeadless <home>\ghidra-projects OpenWheelsReagent -import binary/.../libMyGame.so -overwrite

# 2. Doctor + reverse a function
re-agent doctor
re-agent reverse --address <ADDR>

# 3. Batch work
re-agent plan
re-agent reverse --class CharacterB2D --max-functions 5
```

## Legal note

The Happy Wheels binaries, art, sounds, and level data are © Fancy Force.
OpenWheels recreates engine behavior for interoperability; it does not
redistribute game assets. Original assets are loaded from a user-provided,
legally-obtained copy of the game at runtime (same model as OpenRCT2 / NX1recomp).

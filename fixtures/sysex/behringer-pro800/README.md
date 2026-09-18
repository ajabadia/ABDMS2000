# Behringer Pro-800 SysEx fixtures

These binary fixtures are retained for protocol regression tests. They are not generated project data.

## Files

- `v1.4.4/PRO-800_Presets_v1.4.4.syx`: factory preset dump identified as firmware 1.4.4; expected format version v111, 100 messages and 173 decoded bytes per patch.
- `legacy/Behringer_Pro-800_Factory_Presets.syx`: older factory dump with 101 SysEx messages; the parser observes 98 v109 records with decoded lengths of 155–166 bytes and 3 v110 records of 168 bytes. Exact firmware/version and redistribution terms require confirmation.

## Provenance

The files were supplied locally by the project author and previously stored under `DOCS/DOCS-pro800-borrar/`. The accompanying protocol notes and MIDI/CC table remain under `DOCS/DOCS-pro800-borrar/` as research material until their licensing and upstream references are recorded.

Do not treat these fixtures as proof of hardware compatibility. Tests against them establish parser/regression compatibility only; hardware-tested status requires a physical Pro-800 and recorded firmware version.

# Findings & Decisions

## Requirements
- 128 factory payloads of 254 bytes must roundtrip exactly without edits.
- Edited parameters must not modify unrelated bytes/bits.
- Keep MS2000 and microKORG format distinction; do not change existing SysEx frame/packing.
- Preserve all pre-existing shared-checkout changes.

## Research Findings
- FACTORY_ROM_128_B64 has 32,512 bytes (128 × 254).
- Existing encodeHardwareProgram254 does not write several decoded fields, zeros reserved bytes, conflates amp flags, uses global modSeqResolution for T2, clamps EQ conversion incorrectly, and builds a SysEx preview from scratch.
- Baseline test uxMasterPlanParity.test.js already has working-tree changes for KeySync and parameter parity; preserve them.
- A preliminary uncontextualized comparison showed all 128 generated payloads differed (2,515 bytes total); representative offsets include unused global 0x0C, EQ 0x1A–0x1D and timbre fields.

## Technical Decisions
| Decision | Rationale |
|---|---|
| Each catalog item stores an independent Uint8Array(254) raw254 | Avoid shared ROM views and preserve source-specific representations |
| Contextual encoder starts from raw254 and updates only parameters different from source params | Keeps unknown bits and quantized wire values stable |
| No-context encoder retains compatibility for callers creating fresh programs | Existing UI utilities/tests rely on defaults |

## Issues Encountered
| Issue | Resolution |
|---|---|
| code_search may fail on this Windows workspace due to missing rg binary | Use git grep, Node scripts, and file reads instead |
| Initial inline Node file creation failed due to shell interpolation | Recreated this ledger with a literal heredoc |

## Resources
- WebUI/src/contracts/factoryPresets.js
- WebUI/tests/unit/uxMasterPlanParity.test.js
- WebUI/src/ui/lcdProgrammer.js
- WebUI/src/app.js
- DOCS/MS2000_SysEx_Spec.md
- Source/MIDI/MS2000HardwareProgram.h

# Task Plan: MS2000 Factory Preset Roundtrip

## Goal
Preserve all 254 original bytes for each of the 128 MS2000 factory presets until a represented parameter changes, and update only fields intentionally edited.

## Next Step
None — implementation, verification, and diff review are complete.

## Current Phase
Phase 4

## Phases
### Phase 1: Tests
- [x] Raw preset identity, exact contextual roundtrip, selective field edits
- **Status:** complete
### Phase 2: Contextual encoder
- [x] Store raw payload and selectively encode changed fields
- **Status:** complete
### Phase 3: Export paths
- [x] Pass active source program through inspectors and refresh raw payload after WRITE
- **Status:** complete
### Phase 4: Verification
- [x] Focused and full Vitest suite, diff review
- **Status:** complete

## Decisions Made
| Decision | Rationale |
|---|---|
| Scope to raw MS2000 254-byte payload; microKORG/trame untouched | User clarified that SysEx payloads differ |
| Use each preset raw payload as baseline and compare against source params | Lossless no-edit output while preserving untouched/opaque fields during edits |
| Layout changes between synth/vocoder use fresh encoding; ordinary edits keep untouched raw bytes | Timbre offsets/meaning differ between the two modes |

## Errors Encountered
| Error | Resolution |
|---|---|
| Initial inline Node file creation failed because shell expanded Markdown backticks | Recreated with a single-quoted heredoc |
| git diff --check reported extra blank lines at EOF in files with pre-existing edits | Normalized trailing whitespace to a single newline; did not alter other content |

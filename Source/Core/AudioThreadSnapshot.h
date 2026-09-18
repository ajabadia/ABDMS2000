#pragma once

/*
 * Compatibility shim — canonical implementation lives in ABDSharedCode::SynthCore
 * (namespace abd::synth). Thin re-export keeping historical include paths while
 * consuming the shared module (Phase 2 DRY extraction).
 * NOTE: sync artifact, not hand-maintained — edit ABDSharedCode/SynthCore instead.
 */

#include "SynthCore/AudioThreadSnapshot.h"

namespace ABDMS2000
{
    using abd::synth::AudioThreadSnapshot;
}

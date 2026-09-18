#pragma once

/*
 * Compatibility shim — canonical implementation lives in ABDSharedCode::SynthCore
 * (namespace abd::synth). Thin re-export keeping historical include paths while
 * consuming the shared module (Phase 2 DRY extraction).
 * NOTE: sync artifact, not hand-maintained — edit ABDSharedCode/SynthCore instead.
 */

#include "SynthCore/EnvelopeCurves.h"

namespace ABDMS2000
{
    // Historical code calls EnvelopeCurves::getAttackTimeSeconds(...) inside
    // namespace ABDMS2000 — re-export as a nested namespace, not a flat using.
    namespace EnvelopeCurves
    {
        using namespace abd::synth::EnvelopeCurves;
    }
}

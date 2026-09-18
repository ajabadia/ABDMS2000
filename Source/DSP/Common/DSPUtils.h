#pragma once

/*
 * Compatibility shim — canonical implementation lives in ABDSharedCode::SynthCore
 * (namespace abd::synth, sub-namespace DSPUtils). Thin re-export keeping
 * historical include paths while consuming the shared module (Phase 2 DRY).
 * NOTE: sync artifact, not hand-maintained — edit ABDSharedCode/SynthCore instead.
 */

#include "SynthCore/DSPUtils.h"

namespace ABDMS2000
{
    // Historical code calls DSPUtils::clamp(...) / DSPUtils::TWO_PI inside
    // namespace ABDMS2000 — re-export as a nested namespace, not a flat using.
    namespace DSPUtils
    {
        using namespace abd::synth::DSPUtils;
    }
}

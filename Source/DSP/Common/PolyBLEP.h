#pragma once

/*
 * Compatibility shim — canonical implementation lives in ABDSharedCode::SynthCore
 * (namespace abd::synth). This file is a thin re-export so the synth keeps its
 * historical include paths while consuming the shared module.
 * Extracted: Phase 2 of the transversal DRY plan (ANALISIS_DRY_COMPARTIDO.md §4.1).
 * NOTE: sync artifact, not hand-maintained — edit ABDSharedCode/SynthCore instead.
 */

#include "SynthCore/PolyBLEP.h"

namespace ABDMS2000
{
    using abd::synth::PolyBLEP;
}

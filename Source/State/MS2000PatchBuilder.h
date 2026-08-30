#pragma once
#include "ParameterRegistry.gen.h"
#if ABD_HAS_JUCE
#include <juce_audio_processors/juce_audio_processors.h>
#endif
#include <random>

namespace ABDMS2000 {

#if ABD_HAS_JUCE
/**
 * @brief Intelligent Patch Builder and Musical Randomizer for Korg MS2000.
 */
class MS2000PatchBuilder 
{
public:
    MS2000PatchBuilder() = default;
    ~MS2000PatchBuilder() = default;

    /**
     * Resets the synthesizer parameters to the canonical "Init Synth" state.
     */
    static void buildInitPatch(juce::AudioProcessorValueTreeState& apvts)
    {
        // Voice & Portamento
        setParam(apvts, ParamIDs::portamentoTime, 0.0f);

        // Oscillators (OSC1 Saw wave at full volume, OSC2 off)
        setParam(apvts, ParamIDs::osc1Wave, 0.0f); // 0 = Saw
        setParam(apvts, ParamIDs::osc1Ctrl1, 0.0f);
        setParam(apvts, ParamIDs::osc1DwgsWave, 0.0f);

        setParam(apvts, ParamIDs::osc2Wave, 0.0f); // Saw
        setParam(apvts, ParamIDs::osc2ModType, 0.0f); // Off
        setParam(apvts, ParamIDs::osc2Semitone, 0.0f);
        setParam(apvts, ParamIDs::osc2Tune, 0.0f);

        // Mixer
        setParam(apvts, ParamIDs::mixOsc1Level, 127.0f);
        setParam(apvts, ParamIDs::mixOsc2Level, 0.0f);
        setParam(apvts, ParamIDs::mixNoiseLevel, 0.0f);

        // Filter (24dB LPF open, no resonance, neutral modulations)
        setParam(apvts, ParamIDs::filterType, 0.0f); // 24dB LPF
        setParam(apvts, ParamIDs::filterCutoff, 127.0f); // 100% open
        setParam(apvts, ParamIDs::filterResonance, 0.0f);
        setParam(apvts, ParamIDs::filterEg1Int, 0.0f); // Center 0
        setParam(apvts, ParamIDs::filterKeyTrack, 0.0f);

        // Amp (VCA full level, centered pan, distortion off)
        setParam(apvts, ParamIDs::ampLevel, 100.0f);
        setParam(apvts, ParamIDs::ampPan, 0.0f);
        setParam(apvts, ParamIDs::ampDistortion, 0.0f);
        setParam(apvts, ParamIDs::ampKeyTrack, 0.0f);

        // EG1 (Filter envelope)
        setParam(apvts, ParamIDs::eg1Attack, 0.0f);
        setParam(apvts, ParamIDs::eg1Decay, 64.0f);
        setParam(apvts, ParamIDs::eg1Sustain, 0.0f);
        setParam(apvts, ParamIDs::eg1Release, 32.0f);

        // EG2 (Amp envelope with full sustain and clean release)
        setParam(apvts, ParamIDs::eg2Attack, 0.0f);
        setParam(apvts, ParamIDs::eg2Decay, 40.0f);
        setParam(apvts, ParamIDs::eg2Sustain, 127.0f);
        setParam(apvts, ParamIDs::eg2Release, 20.0f);

        // LFOs
        setParam(apvts, ParamIDs::lfo1Wave, 2.0f); // Triangle
        setParam(apvts, ParamIDs::lfo1KeySync, 2.0f); // Voice
        setParam(apvts, ParamIDs::lfo1Freq, 45.0f);

        setParam(apvts, ParamIDs::lfo2Wave, 2.0f); // Sine
        setParam(apvts, ParamIDs::lfo2KeySync, 2.0f);
        setParam(apvts, ParamIDs::lfo2Freq, 64.0f);

        // Virtual Patch 1-4 (Clear intensities)
        setParam(apvts, ParamIDs::patch1Source, 0.0f);
        setParam(apvts, ParamIDs::patch1Destination, 4.0f);
        setParam(apvts, ParamIDs::patch1Intensity, 0.0f);

        setParam(apvts, ParamIDs::patch2Source, 1.0f);
        setParam(apvts, ParamIDs::patch2Destination, 4.0f);
        setParam(apvts, ParamIDs::patch2Intensity, 0.0f);

        setParam(apvts, ParamIDs::patch3Source, 2.0f);
        setParam(apvts, ParamIDs::patch3Destination, 0.0f);
        setParam(apvts, ParamIDs::patch3Intensity, 0.0f);

        setParam(apvts, ParamIDs::patch4Source, 3.0f);
        setParam(apvts, ParamIDs::patch4Destination, 7.0f);
        setParam(apvts, ParamIDs::patch4Intensity, 0.0f);

        // Effects
        setParam(apvts, ParamIDs::modFxOn, 1.0f);
        setParam(apvts, ParamIDs::modFxType, 0.0f); // Chorus
        setParam(apvts, ParamIDs::delayOn, 1.0f);
        setParam(apvts, ParamIDs::delayType, 0.0f);

        // Arp
        setParam(apvts, ParamIDs::arpOn, 0.0f);
        setParam(apvts, ParamIDs::arpLatch, 0.0f);

        // Vocoder (Off by default for Init Synth)
        setParam(apvts, ParamIDs::synthVocoderMode, 0.0f);
    }

    /**
     * Musical Bounded Randomizer Algorithm:
     * Generates cohesive, instantly playable patches (Basses, Leads, Pads, Arps)
     * avoiding digital silence, clicks or screeching feedback.
     */
    static void buildMusicalRandomPatch(juce::AudioProcessorValueTreeState& apvts)
    {
        juce::Random r;

        // 1. Oscillators: ensure audible tone
        setParam(apvts, ParamIDs::osc1Wave, static_cast<float>(r.nextInt(6))); // Saw, Pulse, Tri, Sin, Vox, DWGS
        setParam(apvts, ParamIDs::osc1Ctrl1, r.nextFloat() * 127.0f);
        setParam(apvts, ParamIDs::osc1DwgsWave, static_cast<float>(r.nextInt(64)));

        setParam(apvts, ParamIDs::osc2Wave, static_cast<float>(r.nextInt(3)));
        setParam(apvts, ParamIDs::osc2ModType, static_cast<float>(r.nextInt(4)));
        setParam(apvts, ParamIDs::osc2Semitone, static_cast<float>(r.nextInt(25) - 12)); // +/- 12 semitones
        setParam(apvts, ParamIDs::osc2Tune, static_cast<float>(r.nextInt(41) - 20));

        // Mixer: HOT range for OSC1 (80..127), optional OSC2 and subtle noise
        setParam(apvts, ParamIDs::mixOsc1Level, r.nextFloat() * 47.0f + 80.0f);
        setParam(apvts, ParamIDs::mixOsc2Level, r.nextFloat() * 127.0f);
        setParam(apvts, ParamIDs::mixNoiseLevel, r.nextFloat() * 30.0f); // Max 30 noise

        // 2. Filter: Bounded sweet spot (Cutoff 40..127, Resonance <= 85)
        setParam(apvts, ParamIDs::filterType, static_cast<float>(r.nextInt(4)));
        setParam(apvts, ParamIDs::filterCutoff, r.nextFloat() * 87.0f + 40.0f);
        setParam(apvts, ParamIDs::filterResonance, r.nextFloat() * 85.0f);
        setParam(apvts, ParamIDs::filterEg1Int, static_cast<float>(r.nextInt(81) - 40));
        setParam(apvts, ParamIDs::filterKeyTrack, static_cast<float>(r.nextInt(61) - 30));

        // 3. Envelopes: Coupled ADSR to prevent zero-gating
        setParam(apvts, ParamIDs::eg1Attack, r.nextFloat() * 70.0f);
        setParam(apvts, ParamIDs::eg1Decay, r.nextFloat() * 90.0f + 20.0f);
        setParam(apvts, ParamIDs::eg1Sustain, r.nextFloat() * 127.0f);
        setParam(apvts, ParamIDs::eg1Release, r.nextFloat() * 80.0f + 10.0f);

        float eg2Sustain = r.nextFloat() * 127.0f;
        setParam(apvts, ParamIDs::eg2Sustain, eg2Sustain);
        if (eg2Sustain < 30.0f) {
            setParam(apvts, ParamIDs::eg2Decay, r.nextFloat() * 60.0f + 50.0f); // Extended decay
        } else {
            setParam(apvts, ParamIDs::eg2Decay, r.nextFloat() * 100.0f);
        }
        setParam(apvts, ParamIDs::eg2Attack, r.nextFloat() * 40.0f); // Snappy attack
        setParam(apvts, ParamIDs::eg2Release, r.nextFloat() * 70.0f + 10.0f);

        // 4. LFOs
        setParam(apvts, ParamIDs::lfo1Wave, static_cast<float>(r.nextInt(4)));
        setParam(apvts, ParamIDs::lfo1Freq, r.nextFloat() * 90.0f + 10.0f);
        setParam(apvts, ParamIDs::lfo2Wave, static_cast<float>(r.nextInt(4)));
        setParam(apvts, ParamIDs::lfo2Freq, r.nextFloat() * 90.0f + 10.0f);

        // 5. Virtual Patch Matrix (50% probability per slot, moderate intensity -40..+40)
        auto randomPatchSlot = [&](const char* srcId, const char* destId, const char* intId) {
            if (r.nextBool()) {
                setParam(apvts, srcId, static_cast<float>(r.nextInt(8)));
                setParam(apvts, destId, static_cast<float>(r.nextInt(8)));
                setParam(apvts, intId, static_cast<float>(r.nextInt(81) - 40));
            } else {
                setParam(apvts, intId, 0.0f);
            }
        };

        randomPatchSlot(ParamIDs::patch1Source, ParamIDs::patch1Destination, ParamIDs::patch1Intensity);
        randomPatchSlot(ParamIDs::patch2Source, ParamIDs::patch2Destination, ParamIDs::patch2Intensity);
        randomPatchSlot(ParamIDs::patch3Source, ParamIDs::patch3Destination, ParamIDs::patch3Intensity);
        randomPatchSlot(ParamIDs::patch4Source, ParamIDs::patch4Destination, ParamIDs::patch4Intensity);

        // Subtle 15% overdrive chance
        setParam(apvts, ParamIDs::ampDistortion, (r.nextFloat() > 0.85f) ? 1.0f : 0.0f);
    }

private:
    static void setParam(juce::AudioProcessorValueTreeState& apvts, const char* paramID, float rawValue)
    {
        if (auto* param = apvts.getParameter(paramID))
        {
            float normalValue = param->getNormalisableRange().convertTo0to1(rawValue);
            param->setValueNotifyingHost(normalValue);
        }
    }
};
#endif

} // namespace ABDMS2000

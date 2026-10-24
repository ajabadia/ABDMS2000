#pragma once
#include "ParameterRegistry.gen.h"
#if ABD_HAS_JUCE
#include <juce_audio_processors/juce_audio_processors.h>
#endif

namespace ABDMS2000
{

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
    static void buildInitPatch(juce::AudioProcessorValueTreeState &apvts)
    {
        // Voice & Portamento
        setParam(apvts, ParamIDs::portamentoTime, 0.0f);

        // Oscillators (OSC1 Saw wave at full volume, OSC2 off)
        setParam(apvts, ParamIDs::osc1Wave, 0.0f);  // 0 = Saw
        setParam(apvts, ParamIDs::osc1Ctrl1, 0.0f);
        setParam(apvts, ParamIDs::osc1DwgsWave, 0.0f);

        setParam(apvts, ParamIDs::osc2Wave, 0.0f);     // Saw
        setParam(apvts, ParamIDs::osc2ModType, 0.0f);  // Off
        setParam(apvts, ParamIDs::osc2Semitone, 0.0f);
        setParam(apvts, ParamIDs::osc2Tune, 0.0f);

        // Mixer
        setParam(apvts, ParamIDs::mixOsc1Level, 127.0f);
        setParam(apvts, ParamIDs::mixOsc2Level, 0.0f);
        setParam(apvts, ParamIDs::mixNoiseLevel, 0.0f);

        // Filter (24dB LPF open, no resonance, neutral modulations)
        setParam(apvts, ParamIDs::filterType, 0.0f);      // 24dB LPF
        setParam(apvts, ParamIDs::filterCutoff, 127.0f);  // 100% open
        setParam(apvts, ParamIDs::filterResonance, 0.0f);
        setParam(apvts, ParamIDs::filterEg1Int, 0.0f);  // Center 0
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
        setParam(apvts, ParamIDs::lfo1Wave, 2.0f);     // Triangle
        setParam(apvts, ParamIDs::lfo1KeySync, 2.0f);  // Voice
        setParam(apvts, ParamIDs::lfo1Freq, 45.0f);

        setParam(apvts, ParamIDs::lfo2Wave, 2.0f);  // Sine
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
        setParam(apvts, ParamIDs::modFxType, 0.0f);  // Chorus
        setParam(apvts, ParamIDs::delayOn, 1.0f);
        setParam(apvts, ParamIDs::delayType, 0.0f);

        // Arp
        setParam(apvts, ParamIDs::arpOn, 0.0f);
        setParam(apvts, ParamIDs::arpLatch, 0.0f);

        // Vocoder (Off by default for Init Synth)
        setParam(apvts, ParamIDs::synthVocoderMode, 0.0f);

        // Ensure Master Volume is audible
        if (auto *mv = apvts.getRawParameterValue(ParamIDs::masterVolume))
        {
            if (mv->load() <= 0.01f)
                setParam(apvts, ParamIDs::masterVolume, 0.8f);
        }
    }

    /**
     * Musical Bounded Randomizer Algorithm (adapted from ReMS2000 + musical sweet spots):
     * Generates cohesive, instantly playable patches (Basses, Leads, Pads, Arps)
     * avoiding digital silence (e.g. HPF cutoff clamping, AudioIn skip), clicks, or screeching feedback.
     */
    static void buildMusicalRandomPatch(juce::AudioProcessorValueTreeState &apvts)
    {
        // Use deterministic LCG for WASM compatibility (juce::Random default constructor
        // seeks system entropy which is unavailable in AudioWorklet/WASM).
        uint32_t seed = static_cast<uint32_t>(juce::Time::getMillisecondCounterHiRes() * 1000);
        auto nextFloat = [&seed]() -> float
        {
            seed = seed * 1664525u + 1013904223u;
            return static_cast<float>(seed & 0xFFFFFF) / 16777216.0f;
        };
        auto nextInt = [&seed](int maxExclusive) -> int
        {
            seed = seed * 1664525u + 1013904223u;
            return static_cast<int>(seed % static_cast<uint32_t>(maxExclusive));
        };
        auto nextRangeInt = [&nextInt](int minInclusive, int maxInclusive) -> int
        { return minInclusive + nextInt(maxInclusive - minInclusive + 1); };
        auto nextBool = [&seed]() -> bool
        {
            seed = seed * 1664525u + 1013904223u;
            return (seed & 0x80000000) != 0;
        };

        auto randomizeTimbre = [&](const juce::String &prefix)
        {
            auto pid = [&](const char *base) -> juce::String
            {
                if (prefix.isEmpty())
                    return juce::String(base);
                juce::String s(base);
                return prefix + s.substring(0, 1).toUpperCase() + s.substring(1);
            };
            auto setTParam = [&](const char *base, float val)
            {
                const auto id = pid(base);
                setParam(apvts, id.toRawUTF8(), val);
            };

            // 1. Voice Assign, Detune & Portamento (ReMS2000 bounds)
            setTParam("voiceMode", static_cast<float>(nextRangeInt(0, 2)));      // Mono, Poly, Unison
            setTParam("unisonDetune", static_cast<float>(nextRangeInt(5, 45)));  // Musical unison detune (5..45 cents)
            setTParam("portamentoTime", nextBool() ? static_cast<float>(nextRangeInt(0, 45)) : 0.0f);

            // 2. Oscillators: skip AudioIn (7) per ReMS2000 rndValueMan(0, 6); musical semitone intervals
            setTParam("osc1Wave", static_cast<float>(nextRangeInt(0, 6)));  // Saw..Noise (skip AudioIn)
            setTParam("osc1Ctrl1", static_cast<float>(nextRangeInt(0, 127)));
            setTParam("osc1Ctrl2", static_cast<float>(nextRangeInt(0, 127)));
            setTParam("osc1DwgsWave", static_cast<float>(nextRangeInt(0, 63)));

            setTParam("osc2Wave", static_cast<float>(nextRangeInt(0, 2)));
            const int osc2Mod = (nextFloat() < 0.65f) ? 0 : nextRangeInt(1, 3);
            setTParam("osc2ModType", static_cast<float>(osc2Mod));

            static constexpr int kMusicalIntervals[] = {-12, -7, -5, 0, 0, 7, 12};
            const int semi = (osc2Mod == 0) ? kMusicalIntervals[nextInt(7)] : nextRangeInt(-12, 12);
            setTParam("osc2Semitone", static_cast<float>(semi));
            setTParam("osc2Tune", static_cast<float>(nextRangeInt(-15, 15)));

            // 3. Mixer: ReMS2000 guarantees audible OSC1/OSC2 (45..127) and bounded Noise (* 0.5)
            setTParam("mixOsc1Level", static_cast<float>(nextRangeInt(65, 127)));
            setTParam("mixOsc2Level", static_cast<float>(nextRangeInt(45, 127)));
            setTParam("mixNoiseLevel", static_cast<float>(nextRangeInt(0, 45)));

            // 4. Filter: ReMS2000 HPF rule (if filterType == 3 HPF12, clamp cutoff to 0..40 to avoid thin silence)
            const int fType = nextRangeInt(0, 3);
            setTParam("filterType", static_cast<float>(fType));
            if (fType == 3)
                setTParam("filterCutoff", static_cast<float>(nextRangeInt(0, 40)));
            else
                setTParam("filterCutoff", static_cast<float>(nextRangeInt(35, 127)));

            setTParam("filterResonance", static_cast<float>(nextRangeInt(0, 38)));  // ReMS2000: * 0.3
            setTParam("filterEg1Int", static_cast<float>(nextRangeInt(-25, 25)));   // ReMS2000: * 0.4
            setTParam("filterKeyTrack", static_cast<float>(nextRangeInt(-30, 30)));
            setTParam("filterVelo", static_cast<float>(nextRangeInt(-30, 30)));

            // 5. Amp: ReMS2000 level 110..127, pan * 0.4 (-25..+25)
            setTParam("ampLevel", static_cast<float>(nextRangeInt(110, 127)));
            setTParam("ampPan", static_cast<float>(nextRangeInt(-25, 25)));
            setTParam("ampDistortion", (nextFloat() > 0.80f) ? 1.0f : 0.0f);
            setTParam("ampKeyTrack", static_cast<float>(nextRangeInt(-25, 25)));
            setTParam("ampVelo", static_cast<float>(nextRangeInt(0, 45)));

            // 6. Envelopes: ReMS2000 Attack * 0.3 (0..38) + coupled ADSR sustain/decay
            setTParam("eg1Attack", static_cast<float>(nextRangeInt(0, 38)));
            setTParam("eg1Decay", static_cast<float>(nextRangeInt(20, 110)));
            setTParam("eg1Sustain", static_cast<float>(nextRangeInt(0, 127)));
            setTParam("eg1Release", static_cast<float>(nextRangeInt(10, 90)));

            setTParam("eg2Attack", static_cast<float>(nextRangeInt(0, 38)));
            const int eg2Sus = nextRangeInt(0, 127);
            setTParam("eg2Sustain", static_cast<float>(eg2Sus));
            if (eg2Sus < 35)
                setTParam("eg2Decay", static_cast<float>(nextRangeInt(55, 115)));
            else
                setTParam("eg2Decay", static_cast<float>(nextRangeInt(20, 105)));
            setTParam("eg2Release", static_cast<float>(nextRangeInt(12, 85)));

            // 7. LFOs (ReMS2000)
            setTParam("lfo1Wave", static_cast<float>(nextRangeInt(0, 3)));
            setTParam("lfo1KeySync", static_cast<float>(nextRangeInt(0, 2)));
            setTParam("lfo1TempoSync", nextBool() ? 1.0f : 0.0f);
            setTParam("lfo1SyncNote", static_cast<float>(nextRangeInt(0, 14)));
            setTParam("lfo1Freq", static_cast<float>(nextRangeInt(10, 100)));

            setTParam("lfo2Wave", static_cast<float>(nextRangeInt(0, 3)));
            setTParam("lfo2KeySync", static_cast<float>(nextRangeInt(0, 2)));
            setTParam("lfo2TempoSync", nextBool() ? 1.0f : 0.0f);
            setTParam("lfo2SyncNote", static_cast<float>(nextRangeInt(0, 14)));
            setTParam("lfo2Freq", static_cast<float>(nextRangeInt(10, 100)));

            // 8. Virtual Patch 1-4 (ReMS2000: intensity * 0.5 => -31..+31)
            for (int p = 1; p <= 4; ++p)
            {
                const juce::String pBase = "patch" + juce::String(p);
                setTParam((pBase + "Source").toRawUTF8(), static_cast<float>(nextRangeInt(0, 7)));
                setTParam((pBase + "Destination").toRawUTF8(), static_cast<float>(nextRangeInt(0, 7)));
                const int intensity = nextBool() ? nextRangeInt(-31, 31) : 0;
                setTParam((pBase + "Intensity").toRawUTF8(), static_cast<float>(intensity));
            }

            // 9. Mod Sequencer (ReMS2000 destination-dependent step bounds)
            setTParam("modSeqOn", (nextFloat() < 0.35f) ? 1.0f : 0.0f);
            setTParam("modSeqType", static_cast<float>(nextRangeInt(0, 3)));
            setTParam("modSeqResolution", static_cast<float>(nextRangeInt(2, 8)));
            static constexpr int kStepLengths[] = {4, 8, 12, 16};
            setTParam("seqLastStep", static_cast<float>(kStepLengths[nextInt(4)]));
            setTParam("seqRunMode", static_cast<float>(nextRangeInt(0, 1)));
            setTParam("seqKeySync", static_cast<float>(nextRangeInt(0, 2)));

            for (int lane = 1; lane <= 3; ++lane)
            {
                const int dest = nextRangeInt(0, 30);
                const juce::String lanePrefix = "seq" + juce::String(lane);
                setTParam((lanePrefix + "Dest").toRawUTF8(), static_cast<float>(dest));
                setTParam((lanePrefix + "Motion").toRawUTF8(), static_cast<float>(nextRangeInt(0, 1)));

                // ReMS2000 getSeqStepMaxVal(dest): StepLength(2) => +-6, Pitch(1)/OSC2Semi(6) => +-24, others => +-63
                const int maxStepVal = (dest == 2) ? 6 : ((dest == 1 || dest == 6) ? 24 : 63);
                for (int step = 1; step <= 16; ++step)
                {
                    const juce::String stepKey = lanePrefix + "Step" + juce::String(step);
                    setTParam(stepKey.toRawUTF8(), static_cast<float>(nextRangeInt(-maxStepVal, maxStepVal)));
                }
            }
        };

        // Randomize both Timbre 1 and Timbre 2
        randomizeTimbre("");
        randomizeTimbre("t2");

        // 10. Global Effects & EQ (ReMS2000: EQ gain -4..+12 dB => 60..76 in 0..127 scale)
        setParam(apvts, ParamIDs::eqLowFreq, static_cast<float>(nextRangeInt(0, 3)));
        setParam(apvts, ParamIDs::eqLowGain, static_cast<float>(nextRangeInt(60, 76)));
        setParam(apvts, ParamIDs::eqHighFreq, static_cast<float>(nextRangeInt(0, 3)));
        setParam(apvts, ParamIDs::eqHighGain, static_cast<float>(nextRangeInt(60, 76)));

        setParam(apvts, ParamIDs::modFxOn, (nextFloat() < 0.75f) ? 1.0f : 0.0f);
        setParam(apvts, ParamIDs::modFxType, static_cast<float>(nextRangeInt(0, 2)));
        setParam(apvts, ParamIDs::modFxSpeed, static_cast<float>(nextRangeInt(15, 90)));
        setParam(apvts, ParamIDs::modFxDepth, static_cast<float>(nextRangeInt(25, 95)));

        setParam(apvts, ParamIDs::delayOn, (nextFloat() < 0.70f) ? 1.0f : 0.0f);
        setParam(apvts, ParamIDs::delayType, static_cast<float>(nextRangeInt(0, 2)));
        setParam(apvts, ParamIDs::delayTime, static_cast<float>(nextRangeInt(20, 95)));
        setParam(apvts, ParamIDs::delayDepth, static_cast<float>(nextRangeInt(20, 75)));
        setParam(apvts, ParamIDs::delayFeedback, static_cast<float>(nextRangeInt(15, 75)));

        // 11. Arpeggiator (ReMS2000: Gate 33..127)
        setParam(apvts, ParamIDs::arpOn, (nextFloat() < 0.30f) ? 1.0f : 0.0f);
        setParam(apvts, ParamIDs::arpLatch, nextBool() ? 1.0f : 0.0f);
        setParam(apvts, ParamIDs::arpType, static_cast<float>(nextRangeInt(0, 5)));
        setParam(apvts, ParamIDs::arpRange, static_cast<float>(nextRangeInt(1, 4)));
        setParam(apvts, ParamIDs::arpGate, static_cast<float>(nextRangeInt(33, 127)));
        setParam(apvts, ParamIDs::arpResolution, static_cast<float>(nextRangeInt(2, 7)));
    }

    /**
     * Copies all 115 Timbre 1 parameters into Timbre 2 (`<param>` -> `t2<Param>`).
     */
    static void copyTimbre1ToTimbre2(juce::AudioProcessorValueTreeState &apvts)
    {
        forEachTimbrePair(
            [&](const juce::String &t1Id, const juce::String &t2Id)
            {
                if (auto *p1 = apvts.getRawParameterValue(t1Id))
                    setParam(apvts, t2Id.toRawUTF8(), p1->load());
            });
    }

    /**
     * Copies all 115 Timbre 2 parameters into Timbre 1 (`t2<Param>` -> `<param>`).
     */
    static void copyTimbre2ToTimbre1(juce::AudioProcessorValueTreeState &apvts)
    {
        forEachTimbrePair(
            [&](const juce::String &t1Id, const juce::String &t2Id)
            {
                if (auto *p2 = apvts.getRawParameterValue(t2Id))
                    setParam(apvts, t1Id.toRawUTF8(), p2->load());
            });
    }

    /**
     * Swaps all 115 parameters between Timbre 1 and Timbre 2 (`<param>` <-> `t2<Param>`).
     */
    static void swapTimbres(juce::AudioProcessorValueTreeState &apvts)
    {
        forEachTimbrePair(
            [&](const juce::String &t1Id, const juce::String &t2Id)
            {
                auto *p1 = apvts.getRawParameterValue(t1Id);
                auto *p2 = apvts.getRawParameterValue(t2Id);
                if (p1 != nullptr && p2 != nullptr)
                {
                    const float v1 = p1->load();
                    const float v2 = p2->load();
                    setParam(apvts, t1Id.toRawUTF8(), v2);
                    setParam(apvts, t2Id.toRawUTF8(), v1);
                }
            });
    }

  private:
    template <typename PairFn> static void forEachTimbrePair(PairFn &&fn)
    {
        for (const auto &meta : ParameterRegistry::getAllParameters())
        {
            const juce::String id(meta.id);
            if (id.startsWith("t2") && id.length() > 2)
            {
                const juce::String t1Id = id.substring(2, 3).toLowerCase() + id.substring(3);
                if (ParameterRegistry::getParameter(t1Id.toRawUTF8()) != nullptr)
                    fn(t1Id, id);
            }
        }
    }

    static void setParam(juce::AudioProcessorValueTreeState &apvts, const char *paramID, float rawValue)
    {
        if (auto *param = apvts.getParameter(paramID))
        {
            float normalValue = param->getNormalisableRange().convertTo0to1(rawValue);
            param->setValueNotifyingHost(normalValue);
        }
    }
};
#endif

}  // namespace ABDMS2000

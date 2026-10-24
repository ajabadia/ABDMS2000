#include "WasmBridge.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>
#include "../Core/AudioThreadSnapshot.h"
#include "../Core/HardwareConstants.h"
#include "../Core/MS2000FirmwareTables.h"
#include "../Core/Voice.h"
#include "../Core/VoiceManager.h"
#include "../DSP/Effects/DelayFX.h"
#include "../DSP/Effects/Equalizer.h"
#include "../DSP/Effects/ModFX.h"
#include "../DSP/Sequencer/Arpeggiator.h"
#include "../DSP/Sequencer/ModSequencer.h"
#include "../DSP/Vocoder/Vocoder16Band.h"
#include "../State/MS2000FactoryBank.h"

namespace ABDMS2000
{

class WasmEngineInstance
{
  public:
    WasmEngineInstance()
    {
        voiceParams_.osc1Type = OSC1Type::Saw;
        voiceParams_.osc1Level = 1.0f;
        voiceParams_.filterType = FilterType::LPF24;
        voiceParams_.filterCutoffNorm = 1.0f;
        voiceParams_.filterResonance = 0.0f;
        voiceParams_.eg2Sustain = 1.0f;
        voiceParams_.ampLevel = 0.8f;
        masterVolume_ = 0.8f;

        // Effects defaults matching parameters-spec schema (delayOn=1, modFxOn=1)
        modFX_.setEnabled(true);
        modFX_.setType(ModFXType::ChorusFlanger);
        modFX_.setSpeed(40.0f / 127.0f);
        modFX_.setDepth(64.0f / 127.0f);
        modFX_.setFeedback(0.0f);

        delayFX_.setEnabled(true);
        delayFX_.setType(DelayType::Stereo);
        delayFX_.setTimeSeconds(0.005f + (1.395f * (40.0f / 127.0f) * (40.0f / 127.0f)));
        delayFX_.setDepth(40.0f / 127.0f);
        delayFX_.setFeedback(30.0f / 127.0f);
    }

    void prepare(double sampleRate)
    {
        sampleRate_ = sampleRate;
        voiceManager_.prepare(sampleRate);
        modFX_.prepare(sampleRate);
        delayFX_.prepare(sampleRate);
        masterEQ_.prepare(sampleRate);
        vocoder_.prepare(sampleRate);
        arpeggiator_.prepare(sampleRate);
        modSeq_.prepare(sampleRate);

        // Pre-allocate scope ring buffer
        scopeBuffer_.assign(512, 0.0f);
        scopeIndex_ = 0;
    }

    void process(float *outL, float *outR, int numSamples)
    {
        if (!outL || !outR || numSamples <= 0)
            return;

        // Advance Mod Sequencer & apply track outputs to a block copy of voiceParams_
        modSeq_.advanceClock(numSamples);
        VoiceParameters blockParams = voiceParams_;
        if (modSeq_.isEnabled())
        {
            applyModSeq(blockParams);
        }
        voiceManager_.applyBlockParams(blockParams);

        // Process Arpeggiator Clock
        if (arpeggiator_.isEnabled())
        {
            arpEvents_.clear();
            if (arpeggiator_.processStep(numSamples, arpEvents_))
            {
                for (const auto &ev : arpEvents_)
                {
                    if (ev.isNoteOn)
                        voiceManager_.noteOn(ev.midiNote, ev.velocity);
                    else
                        voiceManager_.noteOff(ev.midiNote);
                }
            }
        }

        float peakL = 0.0f;
        float peakR = 0.0f;

        for (int s = 0; s < numSamples; ++s)
        {
            float left = 0.0f;
            float right = 0.0f;

            voiceManager_.process(left, right);

            if (vocoder_.isEnabled())
            {
                float carrier = (left + right) * 0.5f;
                vocoder_.process(0.0f, carrier, left, right);
            }

            modFX_.process(left, right);
            delayFX_.process(left, right);
            masterEQ_.process(left, right);

            left *= masterVolume_;
            right *= masterVolume_;

            outL[s] = left;
            outR[s] = right;

            peakL = std::max(peakL, std::abs(left));
            peakR = std::max(peakR, std::abs(right));

            // Record scope sample
            scopeBuffer_[scopeIndex_] = left;
            scopeIndex_ = (scopeIndex_ + 1) % 512;
        }

        vuLeft_ = peakL;
        vuRight_ = peakR;
    }

    void setTempoBPM(float bpm)
    {
        voiceParams_.bpm = bpm;
        arpeggiator_.setTempoBPM(bpm);
        modSeq_.setTempoBPM(bpm);
    }

    void noteOn(int noteNumber, float velocity)
    {
        const bool firstNoteOfTimbre = (voiceManager_.getActiveVoiceCount() == 0);
        if (seqKeySync_ != static_cast<int>(ModSeqKeySync::Off))
        {
            if (seqKeySync_ != static_cast<int>(ModSeqKeySync::Timbre) || firstNoteOfTimbre)
            {
                modSeq_.triggerKeySync(ModSequencer::TIMBRE1_TRACK, ModSequencer::TRACKS_PER_TIMBRE);
                if (modSeq_.isEnabled())
                {
                    VoiceParameters blockParams = voiceParams_;
                    applyModSeq(blockParams);
                    voiceManager_.applyBlockParams(blockParams);
                }
            }
        }

        if (arpeggiator_.isEnabled())
            arpeggiator_.noteOn(noteNumber, velocity);
        else
            voiceManager_.noteOn(noteNumber, velocity);
    }

    void noteOff(int noteNumber)
    {
        if (arpeggiator_.isEnabled())
            arpeggiator_.noteOff(noteNumber);
        else
            voiceManager_.noteOff(noteNumber);
    }

    void allNotesOff()
    {
        voiceManager_.allNotesOff();
        arpeggiator_.allNotesOff();
    }

    void setParam(const char *paramId, float value)
    {
        if (!paramId)
            return;
        std::string id(paramId);

        // Arp tempo is not a patch parameter but a global clock setting
        if (id == "arpTempo")
        {
            setTempoBPM(value);
            return;
        }

        if (id == "osc1Wave")
            voiceParams_.osc1Type = static_cast<OSC1Type>(std::min(7, static_cast<int>(value)));
        else if (id == "osc1Ctrl1")
            voiceParams_.osc1Ctrl1 = value / 127.0f;
        else if (id == "osc1Ctrl2")
            voiceParams_.osc1Ctrl2 = value / 127.0f;
        else if (id == "osc1DwgsWave")
        {
            const int maxDwgs = (synthMode_ == 2) ? 511 : 63;
            voiceParams_.osc1DwgsIndex = std::clamp(static_cast<int>(value), 0, maxDwgs);
        }
        else if (id == "osc2Wave")
            voiceParams_.osc2Wave = static_cast<VAWaveform>(std::min(2, static_cast<int>(value)));
        else if (id == "osc2Semitone")
            voiceParams_.osc2Semitone = value;
        else if (id == "osc2Tune")
            voiceParams_.osc2Tune = value;
        else if (id == "osc2ModType")
            voiceParams_.osc2ModMode = static_cast<OSC2ModulationMode>(std::min(3, static_cast<int>(value)));
        else if (id == "mixOsc1Level")
            voiceParams_.osc1Level = value / 127.0f;
        else if (id == "mixOsc2Level")
            voiceParams_.osc2Level = value / 127.0f;
        else if (id == "mixNoiseLevel")
            voiceParams_.noiseLevel = value / 127.0f;
        else if (id == "filterType")
            voiceParams_.filterType = static_cast<FilterType>(std::min(3, static_cast<int>(value)));
        else if (id == "filterCutoff")
            voiceParams_.filterCutoffNorm = value / 127.0f;
        else if (id == "filterResonance")
            voiceParams_.filterResonance = value / 127.0f;
        else if (id == "filterEg1Int")
            voiceParams_.eg1FilterIntensity = value / 63.0f;
        else if (id == "filterKeyTrack")
            voiceParams_.filterKbdTrack = value / 63.0f;
        else if (id == "filterVelo")
            voiceParams_.filterVeloSens = value / 63.0f;
        else if (id == "ampLevel")
            voiceParams_.ampLevel = value / 127.0f;
        else if (id == "ampPan")
            voiceParams_.panpot = value / 64.0f;
        else if (id == "ampDistortion")
            voiceParams_.distortionOn = (value > 0.5f);
        else if (id == "ampVelo")
            voiceParams_.ampVeloSens = value / 63.0f;
        else if (id == "ampKeyTrack")
            voiceParams_.ampKeyTrack = value / 63.0f;
        else if (id == "eg1Attack")
            voiceParams_.eg1Attack = value / 127.0f;
        else if (id == "eg1Decay")
            voiceParams_.eg1Decay = value / 127.0f;
        else if (id == "eg1Sustain")
            voiceParams_.eg1Sustain = value / 127.0f;
        else if (id == "eg1Release")
            voiceParams_.eg1Release = value / 127.0f;
        else if (id == "eg2Attack")
            voiceParams_.eg2Attack = value / 127.0f;
        else if (id == "eg2Decay")
            voiceParams_.eg2Decay = value / 127.0f;
        else if (id == "eg2Sustain")
            voiceParams_.eg2Sustain = value / 127.0f;
        else if (id == "eg2Release")
            voiceParams_.eg2Release = value / 127.0f;
        else if (id == "lfo1Wave")
        {
            const int w = std::clamp(static_cast<int>(value), 0, 3);
            voiceParams_.lfo1Wave = static_cast<LFOWaveform>(w);
        }
        else if (id == "lfo1Freq")
            voiceParams_.lfo1FreqHz = FirmwareTables::getHardwareLfo1FreqHz(value);
        else if (id == "lfo1KeySync")
            voiceParams_.lfo1KeySync = static_cast<KeySyncMode>(std::clamp(static_cast<int>(value), 0, 3));
        else if (id == "lfo1TempoSync")
            voiceParams_.lfo1TempoSync = (value > 0.5f);
        else if (id == "lfo1SyncNote")
            voiceParams_.lfo1SyncNote = std::clamp(static_cast<int>(value), 0, 14);
        else if (id == "lfo2Wave")
        {
            const int w = std::clamp(static_cast<int>(value), 0, 3);
            voiceParams_.lfo2Wave = static_cast<LFOWaveformLFO2>(w);
        }
        else if (id == "lfo2Freq")
            voiceParams_.lfo2FreqHz = FirmwareTables::getHardwareLfo2FreqHz(value);
        else if (id == "lfo2KeySync")
            voiceParams_.lfo2KeySync = static_cast<KeySyncMode>(std::clamp(static_cast<int>(value), 0, 3));
        else if (id == "lfo2TempoSync")
            voiceParams_.lfo2TempoSync = (value > 0.5f);
        else if (id == "lfo2SyncNote")
            voiceParams_.lfo2SyncNote = std::clamp(static_cast<int>(value), 0, 14);
        else if (id == "patch1Source")
            voiceParams_.patchSlots[0].source = static_cast<PatchSource>(std::clamp(static_cast<int>(value), 0, 7));
        else if (id == "patch1Dest" || id == "patch1Destination")
            voiceParams_.patchSlots[0].destination =
                static_cast<PatchDestination>(std::clamp(static_cast<int>(value), 0, 7));
        else if (id == "patch1Int" || id == "patch1Intensity")
            voiceParams_.patchSlots[0].intensity = std::clamp(value / 63.0f, -1.0f, 1.0f);
        else if (id == "patch2Source")
            voiceParams_.patchSlots[1].source = static_cast<PatchSource>(std::clamp(static_cast<int>(value), 0, 7));
        else if (id == "patch2Dest" || id == "patch2Destination")
            voiceParams_.patchSlots[1].destination =
                static_cast<PatchDestination>(std::clamp(static_cast<int>(value), 0, 7));
        else if (id == "patch2Int" || id == "patch2Intensity")
            voiceParams_.patchSlots[1].intensity = std::clamp(value / 63.0f, -1.0f, 1.0f);
        else if (id == "patch3Source")
            voiceParams_.patchSlots[2].source = static_cast<PatchSource>(std::clamp(static_cast<int>(value), 0, 7));
        else if (id == "patch3Dest" || id == "patch3Destination")
            voiceParams_.patchSlots[2].destination =
                static_cast<PatchDestination>(std::clamp(static_cast<int>(value), 0, 7));
        else if (id == "patch3Int" || id == "patch3Intensity")
            voiceParams_.patchSlots[2].intensity = std::clamp(value / 63.0f, -1.0f, 1.0f);
        else if (id == "patch4Source")
            voiceParams_.patchSlots[3].source = static_cast<PatchSource>(std::clamp(static_cast<int>(value), 0, 7));
        else if (id == "patch4Dest" || id == "patch4Destination")
            voiceParams_.patchSlots[3].destination =
                static_cast<PatchDestination>(std::clamp(static_cast<int>(value), 0, 7));
        else if (id == "patch4Int" || id == "patch4Intensity")
            voiceParams_.patchSlots[3].intensity = std::clamp(value / 63.0f, -1.0f, 1.0f);
        else if (id == "portamentoTime")
            voiceParams_.portamentoTime = value;
        else if (id == "pitchBend")
            voiceParams_.pitchBendValue = std::clamp(value, -1.0f, 1.0f);
        else if (id == "modWheel")
            voiceParams_.modWheelValue = std::clamp(value / 127.0f, 0.0f, 1.0f);
        else if (id == "voiceMode")
            voiceManager_.setAssignMode(static_cast<VoiceAssignMode>(std::min(2, static_cast<int>(value))));
        else if (id == "synthMode")
        {
            synthMode_ = static_cast<int>(value);
            size_t poly = (synthMode_ == 2) ? VoiceManager::MAX_EXPANDED_VOICES : VoiceManager::NUM_HW_VOICES;
            voiceManager_.setMaxPolyphony(poly);
            if (synthMode_ != 2 && voiceParams_.osc1DwgsIndex > 63)
            {
                voiceParams_.osc1DwgsIndex = 63;
            }
            if (synthMode_ == 1)
            {
                modSeq_.setEnabled(false);
            }
            arpeggiator_.setStepMaskEnabled(synthMode_ != 0);
        }
        else if (id == "unisonDetune")
            voiceManager_.setUnisonDetune(value);
        else if (id == "unisonSpread")
            voiceManager_.setUnisonSpread(value);
        else if (id == "modFxOn")
            modFX_.setEnabled(value > 0.5f);
        else if (id == "modFxType")
            modFX_.setType(static_cast<ModFXType>(std::min(2, static_cast<int>(value))));
        else if (id == "modFxSpeed")
            modFX_.setSpeed(value / 127.0f);
        else if (id == "modFxDepth")
            modFX_.setDepth(value / 127.0f);
        else if (id == "modFxFeedback")
            modFX_.setFeedback(value);
        else if (id == "delayOn")
            delayFX_.setEnabled(value > 0.5f);
        else if (id == "delayType")
            delayFX_.setType(static_cast<DelayType>(std::min(2, static_cast<int>(value))));
        else if (id == "delayTime")
            delayFX_.setTimeSeconds(0.005f + (1.395f * (value / 127.0f) * (value / 127.0f)));
        else if (id == "delayDepth")
            delayFX_.setDepth(value / 127.0f);
        else if (id == "delayFeedback")
            delayFX_.setFeedback(value / 127.0f);
        else if (id == "synthVocoderMode")
            vocoder_.setEnabled(value > 0.5f);
        else if (id == "vocoderFormantShift")
            vocoder_.setFormantShift(static_cast<int>(value) - 2);
        else if (id == "vocoderCutoff")
            vocoder_.setCutoffOffset(value / 63.0f);
        else if (id == "vocoderResonance")
            vocoder_.setResonance(value / 127.0f);
        else if (id == "vocoderModSource")
            vocoder_.setModSource(static_cast<int>(value));
        else if (id == "vocoderModInt")
            vocoder_.setModIntensity(value / 63.0f);
        else if (id == "vocoderEfSense")
            vocoder_.setEfSense(value / 127.0f);
        else if (id == "vocoderHold")
            vocoder_.setFormantHold(value > 0.5f);
        else if (id == "vocoderHpfLevel")
            vocoder_.setHPFLevel(value / 127.0f);
        else if (id == "vocoderHpfGate")
            vocoder_.setHPFGate(value > 0.5f);
        else if (id == "vocoderGateSense")
            vocoder_.setGateSense(value / 127.0f);
        else if (id == "vocoderThreshold")
            vocoder_.setHPFThreshold(value / 127.0f);
        else if (id == "vocoderDirectLevel")
            vocoder_.setDirectLevel(value / 127.0f);
        else if (id.rfind("vocoderBandLevel", 0) == 0)
        {
            const int b = std::atoi(id.c_str() + 16) - 1;
            if (b >= 0 && b < 16)
                vocoder_.setBandLevel(static_cast<size_t>(b), value / 127.0f);
        }
        else if (id.rfind("vocoderBandPan", 0) == 0)
        {
            const int b = std::atoi(id.c_str() + 14) - 1;
            if (b >= 0 && b < 16)
                vocoder_.setBandPan(static_cast<size_t>(b), std::clamp((value + 63.0f) / 126.0f, 0.0f, 1.0f));
        }
        else if (id == "masterVolume")
            masterVolume_ = (value <= 1.0f) ? value : (value / 127.0f);

        // EQ parameters
        else if (id == "eqLowFreq")
            masterEQ_.setLowFreqIndex(static_cast<int>(value));
        else if (id == "eqLowGain")
            masterEQ_.setLowGainDB((value - 64.0f) * 12.0f / 63.0f);
        else if (id == "eqHighFreq")
            masterEQ_.setHighFreqIndex(static_cast<int>(value));
        else if (id == "eqHighGain")
            masterEQ_.setHighGainDB((value - 64.0f) * 12.0f / 63.0f);

        // Arpeggiator parameters
        else if (id == "arpOn")
            arpeggiator_.setEnabled(value > 0.5f);
        else if (id == "arpType")
            arpeggiator_.setType(static_cast<ArpType>(std::min(5, static_cast<int>(value))));
        else if (id == "arpRange")
            arpeggiator_.setOctaveRange(static_cast<int>(value));
        else if (id == "arpGate")
            arpeggiator_.setGateTime(value / 127.0f);
        else if (id == "arpLatch")
            arpeggiator_.setLatch(value > 0.5f);
        else if (id == "arpResolution")
        {
            int idx = static_cast<int>(value);
            arpeggiator_.setSyncResolution(idx);
        }
        else if (id == "arpLastStep")
            arpeggiator_.setLastStep(static_cast<int>(value));
        else if (id == "arpStep1")
            arpeggiator_.setStepMask(0, value > 0.5f);
        else if (id == "arpStep2")
            arpeggiator_.setStepMask(1, value > 0.5f);
        else if (id == "arpStep3")
            arpeggiator_.setStepMask(2, value > 0.5f);
        else if (id == "arpStep4")
            arpeggiator_.setStepMask(3, value > 0.5f);
        else if (id == "arpStep5")
            arpeggiator_.setStepMask(4, value > 0.5f);
        else if (id == "arpStep6")
            arpeggiator_.setStepMask(5, value > 0.5f);
        else if (id == "arpStep7")
            arpeggiator_.setStepMask(6, value > 0.5f);
        else if (id == "arpStep8")
            arpeggiator_.setStepMask(7, value > 0.5f);

        // Mod Sequencer parameters
        else if (id == "modSeqOn")
        {
            const bool allow = (synthMode_ != 1);
            modSeq_.setEnabled(allow && (value > 0.5f));
        }
        else if (id == "modSeqType" || id == "seqMotionType")
        {
            ModSeqMode mode = static_cast<ModSeqMode>(std::min(3, static_cast<int>(value)));
            for (int t = 0; t < 6; ++t)
            {
                modSeq_.getTrack(t).mode = mode;
            }
        }
        else if (id == "seqLastStep")
        {
            const int len = std::clamp(static_cast<int>(value), 1, 16);
            seqLastStep_ = len;
            for (int t = 0; t < 6; ++t)
            {
                modSeq_.getTrack(t).length = len;
            }
        }
        else if (id == "seq1Knob" || id == "seq2Knob" || id == "seq3Knob")
        {
            const int row = (id[3] - '1');
            const int destIdx = std::clamp(static_cast<int>(value), 0, 30);
            modSeq_.getTrack(row).destination = static_cast<ModSeqDest>(destIdx);
        }
        else if (id == "seq1Motion" || id == "seq2Motion" || id == "seq3Motion")
        {
            const int row = (id[3] - '1');
            modSeq_.getTrack(row).motion = static_cast<ModSeqMotion>(std::clamp(static_cast<int>(value), 0, 1));
        }
        else if (id.size() >= 9 && id.rfind("seq", 0) == 0 && id.compare(4, 4, "Step") == 0)
        {
            const int row = id[3] - '1';
            const int step = std::atoi(id.c_str() + 8) - 1;
            if (row >= 0 && row < 3 && step >= 0 && step < 16)
            {
                modSeq_.getTrack(row).steps[step] = std::clamp((value + 63.0f) / 126.0f, 0.0f, 1.0f);
            }
        }
        else if (id == "scaleType")
        {
            voiceParams_.scaleType = std::clamp(static_cast<int>(value), 0, 9);
        }
        else if (id == "scaleKey")
        {
            voiceParams_.scaleKey = std::clamp(static_cast<int>(value), 0, 11);
        }
        else if (id == "seqRunMode")
        {
            const bool oneShot = (value < 0.5f);  // 0 = 1Shot, 1 = Loop
            for (int t = 0; t < 6; ++t)
            {
                modSeq_.getTrack(t).oneShot = oneShot;
            }
        }
        else if (id == "modSeqResolution")
        {
            modSeq_.setSyncResolution(static_cast<int>(value));
        }
        else if (id == "seqKeySync")
        {
            seqKeySync_ = std::clamp(static_cast<int>(value), 0, 3);
        }
        else if (id == "ampGate")
            voiceParams_.ampGate = (value > 0.5f);
        else if (id == "pitchTranspose")
            voiceParams_.pitchTranspose = std::clamp(value, -24.0f, 24.0f);
        else if (id == "pitchTune")
            voiceParams_.pitchTune = std::clamp(value, -50.0f, 50.0f);
        else if (id == "pitchBendRange")
            voiceParams_.pitchBendRange = std::clamp(value, -12.0f, 12.0f);
        else if (id == "vibratoInt")
            voiceParams_.vibratoInt = std::clamp(value / 63.0f, -1.0f, 1.0f);
        else if (id == "seq1Dest" || id == "seq2Dest" || id == "seq3Dest")
        {
            const int row = (id[3] - '1');
            const int destIdx = std::clamp(static_cast<int>(value), 0, 30);
            modSeq_.getTrack(row).destination = static_cast<ModSeqDest>(destIdx);
        }
    }

    void applyModSeq(VoiceParameters &vp)
    {
        vp.seq = {};
        bool lengthModulated = false;
        float lengthAmount = 0.0f;

        for (size_t row = 0; row < ModSequencer::TRACKS_PER_TIMBRE; ++row)
        {
            const ModSeqDest dest = modSeq_.getTrack(row).destination;
            if (dest == ModSeqDest::None)
                continue;

            const float value = std::clamp((modSeq_.getTrackOutput(row) - 0.5f) * 2.0f, -1.0f, 1.0f);

            switch (dest)
            {
            case ModSeqDest::Pitch:
                vp.seq.pitch = value;
                break;
            case ModSeqDest::OSC2Semi:
                vp.seq.osc2Pitch = value;
                break;
            case ModSeqDest::OSC2Tune:
                vp.seq.osc2Tune = value * 0.5f;
                break;
            case ModSeqDest::OSC1Ctrl1:
                vp.seq.osc1Ctrl1 = value;
                break;
            case ModSeqDest::OSC1Ctrl2:
                vp.seq.osc1Ctrl2 = value;
                break;
            case ModSeqDest::OSC1Level:
                vp.seq.osc1Level = value;
                break;
            case ModSeqDest::OSC2Level:
                vp.seq.osc2Level = value;
                break;
            case ModSeqDest::NoiseLevel:
                vp.seq.noiseLevel = value;
                break;
            case ModSeqDest::Cutoff:
                vp.seq.cutoff = value;
                break;
            case ModSeqDest::AmpLevel:
                vp.seq.amp = value;
                break;
            case ModSeqDest::Panpot:
                vp.seq.pan = value;
                break;
            case ModSeqDest::LFO2Freq:
                vp.seq.lfo2Freq = value;
                break;
            case ModSeqDest::Resonance:
                vp.filterResonance = std::clamp(vp.filterResonance + value, 0.0f, 1.0f);
                break;
            case ModSeqDest::EG1Int:
                vp.eg1FilterIntensity = std::clamp(vp.eg1FilterIntensity + value, -1.0f, 1.0f);
                break;
            case ModSeqDest::KbdTrk:
                vp.filterKbdTrack = std::clamp(vp.filterKbdTrack + value, -1.0f, 1.0f);
                break;
            case ModSeqDest::Portamento:
                vp.portamentoTime = std::clamp(vp.portamentoTime + (value * 64.0f), 0.0f, 127.0f);
                break;
            case ModSeqDest::LFO1Freq:
                vp.lfo1FreqHz = std::clamp(vp.lfo1FreqHz * std::pow(2.0f, value * Constants::kLfoModMaxOctaves),
                                           Constants::kLfoDefaultMinFrequencyHz,
                                           Constants::kLfoHardwareMaxFrequencyHz);
                break;
            case ModSeqDest::EG1Attack:
                vp.eg1Attack = std::clamp(vp.eg1Attack * (1.0f + value), 0.0f, 1.0f);
                break;
            case ModSeqDest::EG1Decay:
                vp.eg1Decay = std::clamp(vp.eg1Decay * (1.0f + value), 0.0f, 1.0f);
                break;
            case ModSeqDest::EG1Sustain:
                vp.eg1Sustain = std::clamp(vp.eg1Sustain * (1.0f + value), 0.0f, 1.0f);
                break;
            case ModSeqDest::EG1Release:
                vp.eg1Release = std::clamp(vp.eg1Release * (1.0f + value), 0.0f, 1.0f);
                break;
            case ModSeqDest::EG2Attack:
                vp.eg2Attack = std::clamp(vp.eg2Attack * (1.0f + value), 0.0f, 1.0f);
                break;
            case ModSeqDest::EG2Decay:
                vp.eg2Decay = std::clamp(vp.eg2Decay * (1.0f + value), 0.0f, 1.0f);
                break;
            case ModSeqDest::EG2Sustain:
                vp.eg2Sustain = std::clamp(vp.eg2Sustain * (1.0f + value), 0.0f, 1.0f);
                break;
            case ModSeqDest::EG2Release:
                vp.eg2Release = std::clamp(vp.eg2Release * (1.0f + value), 0.0f, 1.0f);
                break;
            case ModSeqDest::Patch1Int:
                vp.patchSlots[0].intensity = std::clamp(vp.patchSlots[0].intensity + value, -1.0f, 1.0f);
                break;
            case ModSeqDest::Patch2Int:
                vp.patchSlots[1].intensity = std::clamp(vp.patchSlots[1].intensity + value, -1.0f, 1.0f);
                break;
            case ModSeqDest::Patch3Int:
                vp.patchSlots[2].intensity = std::clamp(vp.patchSlots[2].intensity + value, -1.0f, 1.0f);
                break;
            case ModSeqDest::Patch4Int:
                vp.patchSlots[3].intensity = std::clamp(vp.patchSlots[3].intensity + value, -1.0f, 1.0f);
                break;
            case ModSeqDest::StepLength:
                lengthModulated = true;
                lengthAmount = value;
                break;
            default:
                break;
            }
        }

        if (lengthModulated)
        {
            const int shifted = static_cast<int>(std::round(seqLastStep_ + (lengthAmount * 6.0f)));
            const int length = std::clamp(shifted, 1, 16);
            for (size_t row = 0; row < ModSequencer::TRACKS_PER_TIMBRE; ++row)
                modSeq_.getTrack(row).length = length;
        }
    }

    void getSnapshot(float *scopeOut, float *vuL, float *vuR, int *activeVoices)
    {
        if (scopeOut)
            std::memcpy(scopeOut, scopeBuffer_.data(), 512 * sizeof(float));
        if (vuL)
            *vuL = vuLeft_;
        if (vuR)
            *vuR = vuRight_;
        if (activeVoices)
            *activeVoices = static_cast<int>(voiceManager_.getActiveVoiceCount());
    }

    void loadFactoryPreset(int index)
    {
        static const MS2000FactoryBank factory;
        if (index >= 0 && index < factory.getNumPresets())
        {
            const auto &prog = factory.getPresetProgramData(index);
            prog.forEachParameterValue([this](const char *id, float val) { setParam(id, val); });
        }
    }

  private:
    double sampleRate_ {44100.0};
    int synthMode_ {0};
    int seqLastStep_ {16};
    int seqKeySync_ {0};
    VoiceManager voiceManager_;
    VoiceParameters voiceParams_;
    float masterVolume_ {0.8f};
    ModFX modFX_;
    DelayFX delayFX_;
    Equalizer masterEQ_;
    Vocoder16Band vocoder_;
    Arpeggiator arpeggiator_;
    ModSequencer modSeq_;
    std::vector<ArpNoteEvent> arpEvents_;

    std::vector<float> scopeBuffer_;
    size_t scopeIndex_ {0};
    float vuLeft_ {0.0f};
    float vuRight_ {0.0f};
};

static std::unique_ptr<WasmEngineInstance> g_wasmEngine;

}  // namespace ABDMS2000

extern "C"
{

    WASM_EXPORT void initEngine(double sampleRate)
    {
        if (!ABDMS2000::g_wasmEngine)
        {
            ABDMS2000::g_wasmEngine = std::make_unique<ABDMS2000::WasmEngineInstance>();
        }
        ABDMS2000::g_wasmEngine->prepare(sampleRate);
    }

    WASM_EXPORT void processAudio(float *outL, float *outR, int numSamples)
    {
        if (ABDMS2000::g_wasmEngine)
        {
            ABDMS2000::g_wasmEngine->process(outL, outR, numSamples);
        }
    }

    WASM_EXPORT void noteOn(int noteNumber, float velocity)
    {
        if (ABDMS2000::g_wasmEngine)
        {
            ABDMS2000::g_wasmEngine->noteOn(noteNumber, velocity);
        }
    }

    WASM_EXPORT void noteOff(int noteNumber)
    {
        if (ABDMS2000::g_wasmEngine)
        {
            ABDMS2000::g_wasmEngine->noteOff(noteNumber);
        }
    }

    WASM_EXPORT void allNotesOff()
    {
        if (ABDMS2000::g_wasmEngine)
        {
            ABDMS2000::g_wasmEngine->allNotesOff();
        }
    }

    WASM_EXPORT void setTempoBPM(float bpm)
    {
        if (ABDMS2000::g_wasmEngine)
        {
            ABDMS2000::g_wasmEngine->setTempoBPM(bpm);
        }
    }

    WASM_EXPORT void setParamNormalized(int paramIndex, float normValue)
    {
        // Forwarded as normalized float (0..1) -> 0..127 raw scale
        if (ABDMS2000::g_wasmEngine)
        {
            // Can map via index
        }
    }

    WASM_EXPORT void setParamById(const char *paramId, float rawValue)
    {
        if (ABDMS2000::g_wasmEngine)
        {
            ABDMS2000::g_wasmEngine->setParam(paramId, rawValue);
        }
    }

    WASM_EXPORT void getAudioSnapshot(float *scopeOut512, float *vuL, float *vuR, int *activeVoices)
    {
        if (ABDMS2000::g_wasmEngine)
        {
            ABDMS2000::g_wasmEngine->getSnapshot(scopeOut512, vuL, vuR, activeVoices);
        }
    }

    WASM_EXPORT void loadProgram(int programIndex)
    {
        if (ABDMS2000::g_wasmEngine)
        {
            ABDMS2000::g_wasmEngine->loadFactoryPreset(programIndex);
        }
    }

    WASM_EXPORT void initPatch()
    {
        if (ABDMS2000::g_wasmEngine)
        {
            ABDMS2000::g_wasmEngine->loadFactoryPreset(0);
        }
    }

    WASM_EXPORT void randomizePatch()
    {
        if (ABDMS2000::g_wasmEngine)
        {
            // Randomize
        }
    }
}

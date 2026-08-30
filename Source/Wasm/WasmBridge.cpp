#include "WasmBridge.h"
#include "../Core/VoiceManager.h"
#include "../Core/Voice.h"
#include "../DSP/Effects/ModFX.h"
#include "../DSP/Effects/DelayFX.h"
#include "../DSP/Effects/Equalizer.h"
#include "../DSP/Vocoder/Vocoder16Band.h"
#include "../DSP/Sequencer/Arpeggiator.h"
#include "../DSP/Sequencer/ModSequencer.h"
#include "../State/MS2000FactoryBank.h"
#include "../Core/AudioThreadSnapshot.h"
#include <memory>
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace ABDMS2000 {

class WasmEngineInstance {
public:
    WasmEngineInstance() {
        voiceParams_.osc1Type = OSC1Type::Saw;
        voiceParams_.osc1Level = 1.0f;
        voiceParams_.filterType = FilterType::LPF24;
        voiceParams_.filterCutoffNorm = 1.0f;
        voiceParams_.filterResonance = 0.0f;
        voiceParams_.eg2Sustain = 1.0f;
        voiceParams_.ampLevel = 0.8f;
        masterVolume_ = 0.8f;
    }

    void prepare(double sampleRate) {
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

    void process(float* outL, float* outR, int numSamples) {
        if (!outL || !outR || numSamples <= 0) return;

        voiceManager_.applyBlockParams(voiceParams_);

        // Process Arpeggiator Clock
        if (arpeggiator_.isEnabled()) {
            arpEvents_.clear();
            if (arpeggiator_.processStep(numSamples, arpEvents_)) {
                for (const auto& ev : arpEvents_) {
                    if (ev.isNoteOn) voiceManager_.noteOn(ev.midiNote, ev.velocity);
                    else voiceManager_.noteOff(ev.midiNote);
                }
            }
        }

        // Advance Mod Sequencer
        modSeq_.advanceClock(numSamples);

        float peakL = 0.0f;
        float peakR = 0.0f;

        for (int s = 0; s < numSamples; ++s) {
            float left = 0.0f;
            float right = 0.0f;

            voiceManager_.process(left, right);

            if (vocoder_.isEnabled()) {
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

    void noteOn(int noteNumber, float velocity) {
        if (arpeggiator_.isEnabled()) arpeggiator_.noteOn(noteNumber, velocity);
        else voiceManager_.noteOn(noteNumber, velocity);
    }

    void noteOff(int noteNumber) {
        if (arpeggiator_.isEnabled()) arpeggiator_.noteOff(noteNumber);
        else voiceManager_.noteOff(noteNumber);
    }

    void allNotesOff() {
        voiceManager_.allNotesOff();
        arpeggiator_.allNotesOff();
    }

    void setParam(const char* paramId, float value) {
        if (!paramId) return;
        std::string id(paramId);

        if (id == "osc1Wave") voiceParams_.osc1Type = static_cast<OSC1Type>(std::min(7, static_cast<int>(value)));
        else if (id == "osc1Ctrl1") voiceParams_.osc1Ctrl1 = value / 127.0f;
        else if (id == "osc1DwgsWave") voiceParams_.osc1DwgsIndex = static_cast<int>(value);
        else if (id == "osc2Wave") voiceParams_.osc2Wave = static_cast<VAWaveform>(std::min(2, static_cast<int>(value)));
        else if (id == "osc2Semitone") voiceParams_.osc2Semitone = value;
        else if (id == "osc2Tune") voiceParams_.osc2Tune = value;
        else if (id == "osc2ModType") voiceParams_.osc2ModMode = static_cast<OSC2ModulationMode>(std::min(3, static_cast<int>(value)));
        else if (id == "mixOsc1Level") voiceParams_.osc1Level = value / 127.0f;
        else if (id == "mixOsc2Level") voiceParams_.osc2Level = value / 127.0f;
        else if (id == "mixNoiseLevel") voiceParams_.noiseLevel = value / 127.0f;
        else if (id == "filterType") voiceParams_.filterType = static_cast<FilterType>(std::min(3, static_cast<int>(value)));
        else if (id == "filterCutoff") voiceParams_.filterCutoffNorm = value / 127.0f;
        else if (id == "filterResonance") voiceParams_.filterResonance = value / 127.0f;
        else if (id == "filterEg1Int") voiceParams_.eg1FilterIntensity = value / 63.0f;
        else if (id == "filterKeyTrack") voiceParams_.filterKbdTrack = value / 63.0f;
        else if (id == "ampLevel") voiceParams_.ampLevel = value / 127.0f;
        else if (id == "ampPan") voiceParams_.panpot = value / 64.0f;
        else if (id == "ampDistortion") voiceParams_.distortionOn = (value > 0.5f);
        else if (id == "eg1Attack") voiceParams_.eg1Attack = value / 127.0f;
        else if (id == "eg1Decay") voiceParams_.eg1Decay = value / 127.0f;
        else if (id == "eg1Sustain") voiceParams_.eg1Sustain = value / 127.0f;
        else if (id == "eg1Release") voiceParams_.eg1Release = value / 127.0f;
        else if (id == "eg2Attack") voiceParams_.eg2Attack = value / 127.0f;
        else if (id == "eg2Decay") voiceParams_.eg2Decay = value / 127.0f;
        else if (id == "eg2Sustain") voiceParams_.eg2Sustain = value / 127.0f;
        else if (id == "eg2Release") voiceParams_.eg2Release = value / 127.0f;
        else if (id == "portamentoTime") voiceParams_.portamentoTime = value;
        else if (id == "modFxOn") modFX_.setEnabled(value > 0.5f);
        else if (id == "modFxType") modFX_.setType(static_cast<ModFXType>(std::min(2, static_cast<int>(value))));
        else if (id == "modFxSpeed") modFX_.setSpeed(value / 127.0f);
        else if (id == "modFxDepth") modFX_.setDepth(value / 127.0f);
        else if (id == "delayOn") delayFX_.setEnabled(value > 0.5f);
        else if (id == "delayType") delayFX_.setType(static_cast<DelayType>(std::min(2, static_cast<int>(value))));
        else if (id == "delayTime") delayFX_.setTimeSeconds(0.005f + (1.395f * (value / 127.0f) * (value / 127.0f)));
        else if (id == "delayDepth") delayFX_.setDepth(value / 127.0f);
        else if (id == "delayFeedback") delayFX_.setFeedback(value / 127.0f);
        else if (id == "synthVocoderMode") vocoder_.setEnabled(value > 0.5f);
        else if (id == "masterVolume") masterVolume_ = value / 127.0f;
    }

    void getSnapshot(float* scopeOut, float* vuL, float* vuR, int* activeVoices) {
        if (scopeOut) std::memcpy(scopeOut, scopeBuffer_.data(), 512 * sizeof(float));
        if (vuL) *vuL = vuLeft_;
        if (vuR) *vuR = vuRight_;
        if (activeVoices) *activeVoices = static_cast<int>(voiceManager_.getActiveVoiceCount());
    }

    void loadFactoryPreset(int index) {
        MS2000FactoryBank factory;
        if (index >= 0 && index < factory.getNumPresets()) {
            const auto& prog = factory.getPresetProgramData(index);
            voiceParams_.osc1Type = static_cast<OSC1Type>(std::min(7, static_cast<int>(prog.rawData[0x0E])));
            voiceParams_.osc1Level = prog.rawData[0x18] / 127.0f;
            voiceParams_.osc2Level = prog.rawData[0x19] / 127.0f;
            voiceParams_.noiseLevel = prog.rawData[0x1A] / 127.0f;
            voiceParams_.filterType = static_cast<FilterType>(std::min(3, static_cast<int>(prog.rawData[0x1B])));
            voiceParams_.filterCutoffNorm = prog.rawData[0x1C] / 127.0f;
            voiceParams_.filterResonance = prog.rawData[0x1D] / 127.0f;
            voiceParams_.ampLevel = prog.rawData[0x20] / 127.0f;
            voiceParams_.eg2Attack = prog.rawData[0x28] / 127.0f;
            voiceParams_.eg2Decay = prog.rawData[0x29] / 127.0f;
            voiceParams_.eg2Sustain = prog.rawData[0x2A] / 127.0f;
            voiceParams_.eg2Release = prog.rawData[0x2B] / 127.0f;
        }
    }

private:
    double sampleRate_{ 44100.0 };
    VoiceManager voiceManager_;
    VoiceParameters voiceParams_;
    float masterVolume_{ 0.8f };
    ModFX modFX_;
    DelayFX delayFX_;
    Equalizer masterEQ_;
    Vocoder16Band vocoder_;
    Arpeggiator arpeggiator_;
    ModSequencer modSeq_;
    std::vector<ArpNoteEvent> arpEvents_;

    std::vector<float> scopeBuffer_;
    size_t scopeIndex_{ 0 };
    float vuLeft_{ 0.0f };
    float vuRight_{ 0.0f };
};

static std::unique_ptr<WasmEngineInstance> g_wasmEngine;

} // namespace ABDMS2000

extern "C" {

WASM_EXPORT void initEngine(double sampleRate) {
    if (!ABDMS2000::g_wasmEngine) {
        ABDMS2000::g_wasmEngine = std::make_unique<ABDMS2000::WasmEngineInstance>();
    }
    ABDMS2000::g_wasmEngine->prepare(sampleRate);
}

WASM_EXPORT void processAudio(float* outL, float* outR, int numSamples) {
    if (ABDMS2000::g_wasmEngine) {
        ABDMS2000::g_wasmEngine->process(outL, outR, numSamples);
    }
}

WASM_EXPORT void noteOn(int noteNumber, float velocity) {
    if (ABDMS2000::g_wasmEngine) {
        ABDMS2000::g_wasmEngine->noteOn(noteNumber, velocity);
    }
}

WASM_EXPORT void noteOff(int noteNumber) {
    if (ABDMS2000::g_wasmEngine) {
        ABDMS2000::g_wasmEngine->noteOff(noteNumber);
    }
}

WASM_EXPORT void allNotesOff() {
    if (ABDMS2000::g_wasmEngine) {
        ABDMS2000::g_wasmEngine->allNotesOff();
    }
}

WASM_EXPORT void setParamNormalized(int paramIndex, float normValue) {
    // Forwarded as normalized float (0..1) -> 0..127 raw scale
    if (ABDMS2000::g_wasmEngine) {
        // Can map via index
    }
}

WASM_EXPORT void setParamById(const char* paramId, float rawValue) {
    if (ABDMS2000::g_wasmEngine) {
        ABDMS2000::g_wasmEngine->setParam(paramId, rawValue);
    }
}

WASM_EXPORT void getAudioSnapshot(float* scopeOut512, float* vuL, float* vuR, int* activeVoices) {
    if (ABDMS2000::g_wasmEngine) {
        ABDMS2000::g_wasmEngine->getSnapshot(scopeOut512, vuL, vuR, activeVoices);
    }
}

WASM_EXPORT void loadProgram(int programIndex) {
    if (ABDMS2000::g_wasmEngine) {
        ABDMS2000::g_wasmEngine->loadFactoryPreset(programIndex);
    }
}

WASM_EXPORT void initPatch() {
    if (ABDMS2000::g_wasmEngine) {
        ABDMS2000::g_wasmEngine->loadFactoryPreset(0);
    }
}

WASM_EXPORT void randomizePatch() {
    if (ABDMS2000::g_wasmEngine) {
        // Randomize
    }
}

}

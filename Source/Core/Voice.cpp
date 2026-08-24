#include "Voice.h"
#include <cmath>

namespace ABDMS2000 {

void Voice::prepare(double sampleRate) noexcept
{
    sampleRate_ = (sampleRate > 1000.0) ? sampleRate : 44100.0;
    osc1_.prepare(sampleRate_);
    osc2_.prepare(sampleRate_);
    noiseGen_.reset();
    filter_.prepare(sampleRate_);
    eg1_.prepare(sampleRate_);
    eg2_.prepare(sampleRate_);
    glide_.prepare(sampleRate_);
    reset();
}

void Voice::reset() noexcept
{
    osc1_.reset();
    osc2_.reset();
    filter_.reset();
    eg1_.reset();
    eg2_.reset();
    glide_.reset(static_cast<float>(currentMidiNote_));
    noteAge_ = 0;
    prevMasterPhase_ = 0.0f;
}

void Voice::noteOn(int midiNote, float velocity, bool glideEnabled) noexcept
{
    currentMidiNote_ = midiNote;
    velocity_ = velocity;
    noteAge_ = 0;

    glide_.setTargetNote(static_cast<float>(midiNote), glideEnabled);

    osc1_.reset();
    osc2_.reset();
    filter_.reset();

    eg1_.noteOn(velocity);
    eg2_.noteOn(velocity);
}

void Voice::noteOff() noexcept
{
    eg1_.noteOff();
    eg2_.noteOff();
}

void Voice::stopImmediately() noexcept
{
    eg1_.reset();
    eg2_.reset();
}

bool Voice::isActive() const noexcept
{
    return !eg2_.isIdle();
}

float Voice::getCurrentAmpLevel() const noexcept
{
    return eg2_.getCurrentLevel();
}

void Voice::renderNextSample(float& leftOut, float& rightOut, const VoiceParameters& params) noexcept
{
    if (!isActive())
    {
        return;
    }

    noteAge_++;

    // 1. Calculate Portamento / Pitch
    glide_.setGlideTime(params.portamentoTime);
    float currentPitch = glide_.getNextPitchSemitones();

    // Base pitch for OSC1
    float osc1Freq = DSPUtils::midiNoteToFrequency(currentPitch);
    osc1_.setFrequency(osc1Freq);
    osc1_.setWaveform(params.osc1Wave);
    osc1_.setControl1(params.osc1Ctrl1);

    // Detuned pitch for OSC2
    float osc2Freq = osc1Freq * DSPUtils::semitonesAndCentsToRatio(params.osc2Semitone, params.osc2Tune);
    osc2_.setFrequency(osc2Freq);
    osc2_.setWaveform(params.osc2Wave);

    // Hard Sync check between OSC1 (master) and OSC2 (slave)
    if (params.osc2ModMode == OSC2ModulationMode::Sync || params.osc2ModMode == OSC2ModulationMode::RingSync)
    {
        OSC2Modulator::checkHardSync(osc1_, osc2_, prevMasterPhase_);
    }
    prevMasterPhase_ = osc1_.getPhase();

    // 2. Generate Oscillator Signals
    float osc1Sig = osc1_.getNextSample();
    float osc2Sig = osc2_.getNextSample();

    // Ring Modulation
    osc1Sig = OSC2Modulator::process(osc1Sig, osc2Sig, params.osc2ModMode);

    // Noise Generator
    float noiseSig = (params.noiseLevel > 0.001f) ? noiseGen_.getWhiteNoise() : 0.0f;

    // 3. Mixer Stage
    float mixedAudio = (osc1Sig * params.osc1Level) + (osc2Sig * params.osc2Level) + (noiseSig * params.noiseLevel);

    // 4. Filter Stage with EG1 & Keyboard Tracking Modulation
    eg1_.setAttack(params.eg1Attack);
    eg1_.setDecay(params.eg1Decay);
    eg1_.setSustain(params.eg1Sustain);
    eg1_.setRelease(params.eg1Release);
    float eg1Val = eg1_.getNextSample(); // 0..1

    // Base Cutoff in Hz
    float baseCutoffHz = DSPUtils::convertSysExToCutoffHz(params.filterCutoffNorm);

    // EG1 modulation: shifts cutoff by up to +/- 5 octaves
    float egOffsetOctaves = (eg1Val - 0.5f) * (params.eg1FilterIntensity * 5.0f);
    // Keyboard tracking modulation (relative to C4 / note 60)
    float kbdOffsetOctaves = ((currentPitch - 60.0f) / 12.0f) * params.filterKbdTrack;

    float finalCutoffHz = baseCutoffHz * std::pow(2.0f, egOffsetOctaves + kbdOffsetOctaves);
    filter_.setType(params.filterType);
    filter_.setCutoff(finalCutoffHz);
    filter_.setResonance(params.filterResonance);

    float filteredAudio = filter_.process(mixedAudio);

    // 5. Amp (VCA) Stage with EG2 & Distortion
    eg2_.setAttack(params.eg2Attack);
    eg2_.setDecay(params.eg2Decay);
    eg2_.setSustain(params.eg2Sustain);
    eg2_.setRelease(params.eg2Release);
    float eg2Val = eg2_.getNextSample(); // 0..1

    float vcaOut = filteredAudio * eg2Val * params.ampLevel;

    if (params.distortionOn)
    {
        vcaOut = DSPUtils::ampDistortion(vcaOut, 0.8f);
    }

    // 6. Stereo Panpot Stage (-1.0 to +1.0)
    float pan = DSPUtils::clamp(params.panpot, -1.0f, 1.0f);
    float panAngle = (pan + 1.0f) * (DSPUtils::PI * 0.25f); // 0 to PI/2
    float leftGain = std::cos(panAngle);
    float rightGain = std::sin(panAngle);

    leftOut += vcaOut * leftGain;
    rightOut += vcaOut * rightGain;
}

} // namespace ABDMS2000

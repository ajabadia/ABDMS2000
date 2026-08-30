#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../MIDI/MIDITelemetryManager.h"
#include "../MIDI/SysExManager.h"
#include "../State/MS2000PatchBuilder.h"
#include "../Core/AppLogger.h"

namespace ABDMS2000 {

static struct PluginProcStaticInit {
    PluginProcStaticInit() {
        ABD_LOG("=== [PROCESSOR STATIC INIT] PluginProcessor.cpp static init ===");
    }
} s_procStaticInit;

ABDMS2000AudioProcessor::ABDMS2000AudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), false)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "Parameters", ParameterRegistry::createParameterLayout()),
      engine_(apvts_),
      midiTelemetry_(std::make_unique<MIDITelemetryManager>(apvts_)),
      sysexManager_(std::make_unique<SysExManager>())
{
    ABD_LOG("[PROCESSOR] ABDMS2000AudioProcessor constructor start.");
    ABD_LOG("[PROCESSOR] APVTS layout created with 105 parameters.");
    ABD_LOG("[PROCESSOR] SynthEngine and VoiceManager initialized.");
    ABD_LOG("[PROCESSOR] Building Init Patch into APVTS...");
    MS2000PatchBuilder::buildInitPatch(apvts_);
    ABD_LOG("[PROCESSOR] Init patch built successfully into APVTS.");
}


ABDMS2000AudioProcessor::~ABDMS2000AudioProcessor()
{
    ABD_LOG("[PROCESSOR] ABDMS2000AudioProcessor destructor called.");
    midiTelemetry_.reset();
    sysexManager_.reset();
}

MIDITelemetryManager& ABDMS2000AudioProcessor::getMIDITelemetry() noexcept
{
    return *midiTelemetry_;
}

const juce::String ABDMS2000AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ABDMS2000AudioProcessor::acceptsMidi() const
{
    return true;
}

bool ABDMS2000AudioProcessor::producesMidi() const
{
    return true;
}

bool ABDMS2000AudioProcessor::isMidiEffect() const
{
    return false;
}

double ABDMS2000AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ABDMS2000AudioProcessor::getNumPrograms()
{
    return sysexManager_ ? static_cast<int>(sysexManager_->getBankSize()) : 128;
}

int ABDMS2000AudioProcessor::getCurrentProgram()
{
    return sysexManager_ ? sysexManager_->getActiveProgramIndex() : 0;
}

void ABDMS2000AudioProcessor::setCurrentProgram(int index)
{
    if (sysexManager_)
    {
        sysexManager_->setActiveProgramIndex(index);
        sysexManager_->loadCurrentProgramIntoAPVTS(apvts_);

        // Unless this is specifically a vocoder program (like index 4: A.05 Vocoder), disable vocoder mode
        if (auto* p = apvts_.getParameter(ParamIDs::synthVocoderMode))
        {
            p->setValueNotifyingHost((index == 4) ? 1.0f : 0.0f);
        }
    }
}

const juce::String ABDMS2000AudioProcessor::getProgramName(int index)
{
    if (sysexManager_)
    {
        return juce::String(sysexManager_->getProgram(static_cast<size_t>(index)).getName());
    }
    return "Init Synth";
}

void ABDMS2000AudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    if (sysexManager_)
    {
        auto prog = sysexManager_->getProgram(static_cast<size_t>(index));
        prog.setName(newName.toStdString());
        sysexManager_->setProgram(static_cast<size_t>(index), prog);
    }
}

void ABDMS2000AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    ABD_LOG(juce::String("[PROCESSOR] prepareToPlay called. SampleRate: ") + juce::String(sampleRate) + ", BlockSize: " + juce::String(samplesPerBlock));
    engine_.prepare(sampleRate, samplesPerBlock);
}

void ABDMS2000AudioProcessor::releaseResources()
{
    ABD_LOG("[PROCESSOR] releaseResources called.");
    engine_.reset();
}

bool ABDMS2000AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainOutput = layouts.getMainOutputChannelSet();
    if (mainOutput != juce::AudioChannelSet::mono()
     && mainOutput != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void ABDMS2000AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Check for incoming SysEx messages to parse
    if (sysexManager_)
    {
        for (const auto meta : midiMessages)
        {
            auto msg = meta.getMessage();
            if (msg.isSysEx())
            {
                sysexManager_->parseSysEx(static_cast<const uint8_t*>(msg.getSysExData()),
                                         static_cast<size_t>(msg.getSysExDataSize()),
                                         apvts_);
            }
        }
    }

    if (midiTelemetry_)
    {
        midiTelemetry_->processIncomingMidi(midiMessages);
    }

    engine_.processBlock(buffer, midiMessages, getPlayHead());

    if (midiTelemetry_)
    {
        midiTelemetry_->renderOutgoingMidi(midiMessages);
    }
}



bool ABDMS2000AudioProcessor::hasEditor() const
{
    ABD_LOG("[PROCESSOR] hasEditor query -> returning true");
    return true;
}

juce::AudioProcessorEditor* ABDMS2000AudioProcessor::createEditor()
{
    ABD_LOG("[PROCESSOR] createEditor called -> instantiating ABDMS2000AudioProcessorEditor");
    return new ABDMS2000AudioProcessorEditor(*this);
}

void ABDMS2000AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // 1. Crear una copia instantánea del árbol de parámetros APVTS
    auto state = apvts_.copyState();

    // 2. Adjuntar metadatos de persistencia (programa actual, nombre y estado LCD)
    if (sysexManager_)
    {
        state.setProperty("currentProgramIndex", sysexManager_->getActiveProgramIndex(), nullptr);
        state.setProperty("currentProgramName", juce::String(sysexManager_->getProgram(sysexManager_->getActiveProgramIndex()).getName()), nullptr);
    }

    // 3. Serializar a XML compacto y escribir en bloque binario del DAW
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml != nullptr)
    {
        copyXmlToBinary(*xml, destData);
    }
}

void ABDMS2000AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0) return;

    // 1. Reconstruir XML desde el bloque binario del DAW
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts_.state.getType()))
    {
        auto newTree = juce::ValueTree::fromXml(*xmlState);
        if (newTree.isValid())
        {
            // 2. Reemplazar atómicamente el estado del APVTS
            apvts_.replaceState(newTree);

            // 3. Restaurar metadatos del programa y LCD si existen
            if (sysexManager_ && newTree.hasProperty("currentProgramIndex"))
            {
                int progIdx = static_cast<int>(newTree.getProperty("currentProgramIndex", 0));
                sysexManager_->setActiveProgramIndex(progIdx);

                if (newTree.hasProperty("currentProgramName"))
                {
                    auto name = newTree.getProperty("currentProgramName", "Init Synth").toString();
                    changeProgramName(progIdx, name);
                }
            }
        }
    }
}


} // namespace ABDMS2000

// JUCE Entry Point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    ABD_LOG("[ENTRY] createPluginFilter() called - instantiating new ABDMS2000AudioProcessor.");
    return new ABDMS2000::ABDMS2000AudioProcessor();
}

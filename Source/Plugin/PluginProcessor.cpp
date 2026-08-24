#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ABDMS2000 {

ABDMS2000AudioProcessor::ABDMS2000AudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "Parameters", ParameterRegistry::createParameterLayout()),
      engine_(apvts_)
{
}

ABDMS2000AudioProcessor::~ABDMS2000AudioProcessor()
{
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
    return 1;
}

int ABDMS2000AudioProcessor::getCurrentProgram()
{
    return 0;
}

void ABDMS2000AudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String ABDMS2000AudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void ABDMS2000AudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void ABDMS2000AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engine_.prepare(sampleRate, samplesPerBlock);
}

void ABDMS2000AudioProcessor::releaseResources()
{
    engine_.reset();
}

bool ABDMS2000AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void ABDMS2000AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    engine_.processBlock(buffer, midiMessages);
}

bool ABDMS2000AudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* ABDMS2000AudioProcessor::createEditor()
{
    return new ABDMS2000AudioProcessorEditor(*this);
}

void ABDMS2000AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ABDMS2000AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts_.state.getType()))
    {
        apvts_.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

} // namespace ABDMS2000

// JUCE Entry Point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ABDMS2000::ABDMS2000AudioProcessor();
}

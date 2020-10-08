#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "Music.h"
#include <algorithm>
#include <map>

class ProcessorBase : public AudioProcessor
{
public:
    ProcessorBase() {}

    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(AudioSampleBuffer&, MidiBuffer&) override {}

    AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

    const String getName() const override { return {}; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }

    int getNumPrograms() override { return 0; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const String getProgramName(int) override { return {}; }
    void changeProgramName(int, const String&) override {}

    void getStateInformation(MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProcessorBase)
};

class OscillatorProcessor : public ProcessorBase
{
public:
    OscillatorProcessor(const String name, Music::Metre& metre) : name(name), musicMetre(metre)
    {
        oscillator.setFrequency(1760.0f);
        oscillator.initialise([](float x) { return std::sin(x); }, 128);
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override
    {
        juce::dsp::ProcessSpec spec { sampleRate, static_cast<uint32>(samplesPerBlock) };
        oscillator.prepare(spec);
    }

    void processBlock(AudioSampleBuffer& buffer, MidiBuffer&) override
    {
        for (auto i = 0; i < buffer.getNumSamples(); ++i)
        {
            auto pulseSample = musicMetre.getPulseSample(*this);
            auto currentSample = oscillator.processSample(0.0f);

            currentSample *= pulseSample;

            for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                auto* outBuffer = buffer.getWritePointer(channel);
                outBuffer[i] = currentSample;
            }
        }
    }

    void reset() override
    {
        oscillator.reset();
    }

    const String getName() const override { return name; }

private:
    const String name;
    Music::Metre& musicMetre;

    juce::dsp::Oscillator<float> oscillator;
};

class AudioFileProcessor : public ProcessorBase
{
public:
    AudioFileProcessor(const String name, Music::Metre& metre, AudioFormatManager& formatManager)
        : name(name), musicMetre(metre), formatManager(formatManager)
    {
        int numBytes;
        auto* binaryData = BinaryData::getNamedResource(soundMap[name.toStdString()], numBytes);
        auto* audioFormatReader = formatManager.createReaderFor(std::make_unique<MemoryInputStream>(static_cast<const void*>(binaryData), numBytes, false));
        std::unique_ptr<AudioFormatReader> reader(audioFormatReader);

        if (reader.get() != nullptr)
        {
            audioFileBuffer.setSize(1, bufferSize);
            reader->read(audioFileBuffer.getArrayOfWritePointers(), 1, 0, bufferSize);
            sourceSampleRate = reader->sampleRate;

            auto maxMagnitude = audioFileBuffer.getMagnitude(0, 0, bufferSize);

            jassert(maxMagnitude > 0);
            audioFileBuffer.applyGain(0, 0, bufferSize, 1 / maxMagnitude);

            lookupTable.initialise([&](size_t i) { return audioFileBuffer.getSample(0, (int) i); }, bufferSize);
        }
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override
    {
        tableDelta = sourceSampleRate / sampleRate;
    }

    void processBlock(AudioSampleBuffer& buffer, MidiBuffer&) override
    {
        for (auto i = 0; i < buffer.getNumSamples(); ++i)
        {
            auto pulseSample = musicMetre.getPulseSample(*this);
            auto currentSample = lookupTable.getUnchecked(currentIndex);

            if ((currentIndex + tableDelta) >= bufferSize)
                currentSample = 0.0f;
            else
                currentIndex += tableDelta;

            currentSample *= pulseSample;

            for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                auto* outBuffer = buffer.getWritePointer(channel);
                outBuffer[i] = currentSample;
            }
        }
    }

    void reset() override
    {
        currentIndex = 0.0f;
    }

    const String getName() const override { return name; }

private:
    const String name;
    Music::Metre& musicMetre;

    std::map<std::string, const char*> soundMap {
        { "LP_Jam_Block", "LP_Jam_Block_ogg" },
        { "Fire", "Fire_wav" }
    };

    AudioFormatManager& formatManager;
    AudioSampleBuffer audioFileBuffer;

    juce::dsp::LookupTable<float> lookupTable;

    const unsigned int bufferSize = 1 << 11;
    double sourceSampleRate = 0.0;
    float currentIndex = 0.0f, tableDelta = 0.0f;
};

class SoundStallProcessor : public AudioProcessor
{
public:
    using AudioGraphIOProcessor = AudioProcessorGraph::AudioGraphIOProcessor;
    using Node = AudioProcessorGraph::Node;

    SoundStallProcessor(Music::Metre& metre)
        : musicMetre(metre),
          AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", AudioChannelSet::stereo(), true)),
          mainProcessor(new AudioProcessorGraph()),
          processorSlot1(new AudioParameterChoice("Slot 1", "Sound", processorChoices, 1))
    {
        addParameter(processorSlot1);

        formatManager.registerBasicFormats();
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override
    {
        mainProcessor->setPlayConfigDetails(getMainBusNumInputChannels(),
                                            getMainBusNumOutputChannels(),
                                            sampleRate,
                                            samplesPerBlock);

        mainProcessor->prepareToPlay(sampleRate, samplesPerBlock);

        initialiseGraph();
    }

    void releaseResources() override
    {
        mainProcessor->releaseResources();
    }

    void processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages) override
    {
        updateGraph();

        mainProcessor->processBlock(buffer, midiMessages);
    }

    void reset() override
    {
        if (slot1Node != nullptr)
            slot1Node->getProcessor()->reset();
    }

    AudioProcessorEditor* createEditor() override { return new GenericAudioProcessorEditor(*this); }
    bool hasEditor() const override { return true; }

    const String getName() const override { return "Sound Stall"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const String getProgramName(int) override { return {}; }
    void changeProgramName(int, const String&) override {}

    void getStateInformation(MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    enum class Waveform
    {
        Sine,
        LP_Jam_Block,
        Fire
    };

private:
    void initialiseGraph()
    {
        mainProcessor->clear();

        audioInputNode = mainProcessor->addNode(std::make_unique<AudioGraphIOProcessor>(AudioGraphIOProcessor::audioInputNode));
        audioOutputNode = mainProcessor->addNode(std::make_unique<AudioGraphIOProcessor>(AudioGraphIOProcessor::audioOutputNode));

        connectAudioNodes();
    }

    void updateGraph()
    {
        bool hasChanged = false;

        ReferenceCountedArray<Node> slots;
        slots.add(slot1Node);

        auto waveformIndex = processorSlot1->getIndex();
        auto slot = slots.getUnchecked(0);

        bool shouldAddNode = false;

        auto checkNodeIfExist = [&]() {
            if (slot != nullptr)
            {
                if (slot->getProcessor()->getName() == processorChoices[waveformIndex])
                    return;

                mainProcessor->disconnectNode(slot->nodeID);
                slot->getProcessor()->reset();
            }

            auto isEqual = [&](const auto& p) { return p->getProcessor()->getName() == processorChoices[waveformIndex]; };
            auto found = std::find_if(processorNodePtrs.begin(), processorNodePtrs.end(), isEqual);

            if (found != processorNodePtrs.end())
                slots.set(0, *found);
            else
                shouldAddNode = true;

            hasChanged = true;
        };

        switch (static_cast<Waveform>(waveformIndex))
        {
            case Waveform::Sine:
            {
                checkNodeIfExist();

                if (shouldAddNode)
                {
                    slots.set(0, mainProcessor->addNode(std::make_unique<OscillatorProcessor>(processorChoices[waveformIndex], musicMetre)));
                    processorNodePtrs.push_back(slots.getUnchecked(0));
                }

                break;
            }
            case Waveform::LP_Jam_Block:
            case Waveform::Fire:
            {
                checkNodeIfExist();

                if (shouldAddNode)
                {
                    slots.set(0, mainProcessor->addNode(std::make_unique<AudioFileProcessor>(processorChoices[waveformIndex], musicMetre, formatManager)));
                    processorNodePtrs.push_back(slots.getUnchecked(0));
                }

                break;
            }
            default:
                jassertfalse;
                break;
        }

        if (hasChanged)
        {
            for (auto connection : mainProcessor->getConnections())
                mainProcessor->removeConnection(connection);

            ReferenceCountedArray<Node> activeSlots;

            for (auto slot : slots)
            {
                if (slot != nullptr)
                {
                    activeSlots.add(slot);

                    slot->getProcessor()->setPlayConfigDetails(getMainBusNumInputChannels(),
                                                               getMainBusNumOutputChannels(),
                                                               getSampleRate(),
                                                               getBlockSize());
                }
            }

            if (activeSlots.isEmpty())
            {
                connectAudioNodes();
            }
            else
            {
                for (int i = 0; i < activeSlots.size() - 1; ++i)
                {
                    for (int channel = 0; channel < 2; ++channel)
                        mainProcessor->addConnection({ { activeSlots.getUnchecked(i)->nodeID, channel },
                                                       { activeSlots.getUnchecked(i + 1)->nodeID, channel } });
                }

                for (int channel = 0; channel < 2; ++channel)
                {
                    mainProcessor->addConnection({ { audioInputNode->nodeID, channel },
                                                   { activeSlots.getFirst()->nodeID, channel } });
                    mainProcessor->addConnection({ { activeSlots.getLast()->nodeID, channel },
                                                   { audioOutputNode->nodeID, channel } });
                }
            }

            for (auto node : mainProcessor->getNodes())
                node->getProcessor()->enableAllBuses();
        }

        for (auto node : processorNodePtrs)
        {
            bool shouldBeBypassed = !mainProcessor->isConnected(audioInputNode->nodeID, node->nodeID);
            node->setBypassed(shouldBeBypassed);
        }

        slot1Node = slots.getUnchecked(0);
    }

    void connectAudioNodes()
    {
        for (int channel = 0; channel < 2; ++channel)
            mainProcessor->addConnection({ { audioInputNode->nodeID, channel },
                                           { audioOutputNode->nodeID, channel } });
    }

    Music::Metre& musicMetre;

    AudioFormatManager formatManager;

    StringArray processorChoices { "Sine", "LP_Jam_Block", "Fire" };

    std::vector<Node::Ptr> processorNodePtrs;

    std::unique_ptr<AudioProcessorGraph> mainProcessor;

    AudioParameterChoice* processorSlot1;

    Node::Ptr audioInputNode;
    Node::Ptr audioOutputNode;

    Node::Ptr slot1Node;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundStallProcessor)
};

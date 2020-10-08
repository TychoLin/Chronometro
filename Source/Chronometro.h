#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "Music.h"
#include "SoundStall.h"
#include <list>

class BeatAudioSource : public AudioSource, public ChangeBroadcaster
{
public:
    BeatAudioSource(Music::Metre& metre) : musicMetre(metre), soundStallProcessor(metre)
    {
    }

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override
    {
        musicMetre.audioDeviceSampleRate = sampleRate;
        musicMetre.init();

        soundStallProcessor.prepareToPlay(sampleRate, samplesPerBlockExpected);
    }

    void releaseResources() override {}

    void getNextAudioBlock(const AudioSourceChannelInfo& bufferToFill) override
    {
        if (stopped)
        {
            bufferToFill.clearActiveBufferRegion();
        }
        else
        {
            AudioSampleBuffer localBuffer {
                bufferToFill.buffer->getArrayOfWritePointers(),
                bufferToFill.buffer->getNumChannels(),
                bufferToFill.startSample,
                bufferToFill.numSamples
            };

            soundStallProcessor.processBlock(localBuffer, midiBuffer);

            auto accent = (*musicMetre.currentPulse).getAccent();
            auto accentProcessor = [&](auto sample) -> auto
            {
                return std::tanh((1 + std::log(10 * accent + 1)) * sample);
            };

            if (accent == 0.0f)
                return;

            for (auto i = 0; i < localBuffer.getNumSamples(); ++i)
            {
                for (auto channel = 0; channel < localBuffer.getNumChannels(); ++channel)
                {
                    auto* outBuffer = localBuffer.getWritePointer(channel);
                    outBuffer[i] = accentProcessor(outBuffer[i]);
                }
            }
        }
    }

    void start()
    {
        if (stopped)
        {
            musicMetre.update();
            stopped = false;

            sendChangeMessage();
        }
    }

    void stop()
    {
        if (!stopped)
        {
            stopped = true;
            soundStallProcessor.reset();

            sendChangeMessage();
        }
    }

    bool isPlaying() { return !stopped; }

    AudioProcessorEditor* createEditor() { return soundStallProcessor.createEditor(); }

private:
    Music::Metre& musicMetre;

    MidiBuffer midiBuffer;
    SoundStallProcessor soundStallProcessor;

    bool stopped { true };
};

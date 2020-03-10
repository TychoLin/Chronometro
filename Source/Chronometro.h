#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include <algorithm>

class WavetableOscillator
{
public:
    WavetableOscillator(const AudioSampleBuffer& wavetableToUse)
        : wavetable(wavetableToUse),
          tableSize(wavetable.getNumSamples() - 1)
    {
    }

    void setFrequency(float frequency, float sampleRate)
    {
        auto cyclesPerSample = frequency / sampleRate;
        baseTableDelta = cyclesPerSample * tableSize;
        tableDelta = baseTableDelta;
    }

    void scaleFrequency(float ratio)
    {
        currentIndex = 0.0f;
        tableDelta = baseTableDelta * ratio;
    }

    forcedinline float getNextSample() noexcept
    {
        auto index0 = (unsigned int) currentIndex;
        auto index1 = index0 + 1;

        auto frac = currentIndex - (float) index0;

        auto* table = wavetable.getReadPointer(0);
        auto value0 = table[index0];
        auto value1 = table[index1];

        auto currentSample = value0 + frac * (value1 - value0);

        if ((currentIndex += tableDelta) > tableSize)
            currentIndex -= tableSize;

        return currentSample;
    }

private:
    const AudioSampleBuffer& wavetable;
    const int tableSize;
    float currentIndex = 0.0f, baseTableDelta = 0.0f, tableDelta = 0.0f;
};

class Music
{
public:
    struct Pulse
    {
        Pulse(bool hit, float accent, float timeLength) : index(0), hit(hit), pulseSampleLength(timeLength)
        {
            this->accent = jlimit(0.0f, 1.0f, accent);
        }

        int index;
        bool hit;
        float accent;
        float pulseSampleLength;
    };

    class Metre : public Timer
    {
    public:
        Metre() {}

        void setBPM(float bpmToUse)
        {
            if (bpmToUse < 20.0f)
                BPM = 20.0f;
            else if (bpmToUse > 999.0f)
                BPM = 999.0f;
            else
                BPM = bpmToUse;
        }

        float getBPM()
        {
            return BPM;
        }

        float getIntervalPerBeat()
        {
            return 60.0f / BPM;
        }

        double getSampleRatePerBeat()
        {
            return audioDeviceSampleRate * getIntervalPerBeat();
        }

        void createMetre()
        {
            auto note4th = getSampleRatePerBeat();
            auto note8th = note4th * 0.5;
            auto note16th = note4th * 0.25;

            pulseGroup = {
                Pulse(true, 0.0f, note4th),
                Pulse(true, 0.3f, note4th),
                Pulse(true, 0.0f, note4th),
                Pulse(true, 0.3f, note4th)
            };
        }

        void setTapBPM()
        {
            if (tapTimes.empty())
                startTimer(3000); // bpm=20

            if (tapTimes.size() >= 4)
                tapTimes.erase(tapTimes.begin());

            tapTimes.push_back(Time::getMillisecondCounterHiRes());

            if (tapTimes.size() >= 2)
            {
                auto it = tapTimes.end();
                auto tapEndTime = --it;
                auto tapStartTime = --it;

                if (*tapEndTime - *tapStartTime > 0.0)
                {
                    float interval = (float) (*tapEndTime - *tapStartTime);
                    setBPM(60.0f / (interval * 0.001f));

                    startTimer((int) interval * 1.5f); // refresh the timer based on interval
                }
            }
        }

        double audioDeviceSampleRate;

        std::vector<Pulse> pulseGroup;

    private:
        void timerCallback() override
        {
            tapTimes.clear();
            stopTimer();
        }

        float BPM = 120.0f;
        std::vector<double> tapTimes;
    };

private:
    Music() = delete;
};

class BeatAudioSource : public AudioSource, public ChangeBroadcaster
{
public:
    BeatAudioSource(Music::Metre& metre) : musicMetre(metre)
    {
        createWavetable(Waveform::LP_Jam_Block);
    }

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override
    {
        oscillator.reset(new WavetableOscillator(soundTable));

        // auto frequency = 440.0f;
        // oscillator->setFrequency((float) frequency, (float) sampleRate);

        auto frequency = soundFileSampleRate / soundTable.getNumSamples();
        oscillator->setFrequency((float) frequency, (float) soundFileSampleRate);

        musicMetre.audioDeviceSampleRate = sampleRate;
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
            auto* leftBuffer = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
            auto* rightBuffer = bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample);

            bufferToFill.clearActiveBufferRegion();

            for (auto sample = 0; sample < bufferToFill.numSamples; ++sample)
            {
                auto levelSample = getLevelSample();

                leftBuffer[sample] = levelSample;
                rightBuffer[sample] = levelSample;
            }
        }
    }

    void start()
    {
        if (stopped)
        {
            musicMetre.createMetre();
            stopped = false;

            sendChangeMessage();
        }
    }

    void stop()
    {
        if (!stopped)
        {
            stopped = true;
            tailOff = 1.0;

            sendChangeMessage();
        }
    }

    bool isPlaying() { return !stopped; }

    enum class Waveform
    {
        Sine,
        LP_Jam_Block
    };

private:
    void createWavetable(Waveform w)
    {
        waveform = w;

        switch (waveform)
        {
            case Waveform::Sine:
            {
                soundTable.setSize(1, (int) tableSize + 1);
                auto* samples = soundTable.getWritePointer(0);

                auto angleDelta = MathConstants<double>::twoPi / (double) (tableSize - 1);
                auto currentAngle = 0.0;

                for (unsigned int i = 0; i < tableSize; ++i)
                {
                    auto sample = std::sin(currentAngle);
                    samples[i] = (float) sample;
                    currentAngle += angleDelta;
                }

                samples[tableSize] = samples[0];
            }
            break;
            case Waveform::LP_Jam_Block:
            {
                AudioFormatManager formatManager;
                formatManager.registerBasicFormats();

                auto dir = File::getSpecialLocation(File::SpecialLocationType::invokedExecutableFile);
                String relativeFilePath = "LP_Jam_Block.ogg";

                int numTries = 0;

                while (!dir.getChildFile(relativeFilePath).exists() && numTries++ < 15)
                    dir = dir.getParentDirectory();

                // sample length: 50675
                // channels: 2
                // sample rate: 48000
                std::unique_ptr<AudioFormatReader> reader(formatManager.createReaderFor(dir.getChildFile(relativeFilePath)));

                if (reader.get() != nullptr)
                {
                    soundTable.setSize(reader->numChannels, tableSize);
                    reader->read(&soundTable, 0, tableSize, 0, true, true);
                    soundFileSampleRate = reader->sampleRate;
                }
            }
            break;
            default:
                jassertfalse;
                break;
        }
    }

    float getLevelSample()
    {
        auto currentPulse = musicMetre.pulseGroup.begin();

        if (currentPulse == musicMetre.pulseGroup.end())
            return 0.0f;

        if (currentPulse->index == 0)
            oscillator->scaleFrequency(1 + currentPulse->accent);

        if (currentPulse->index < currentPulse->pulseSampleLength)
        {
            float levelSample;

            if (currentPulse->hit && tailOff > 0.005f)
            {
                levelSample = oscillator->getNextSample() * level;

                if (waveform == Waveform::LP_Jam_Block && currentPulse->index > (int) tableSize / (1 + currentPulse->accent))
                    levelSample = 0.0f;

                if (currentPulse->index > currentPulse->pulseSampleLength * 0.3f)
                {
                    levelSample *= tailOff;
                    tailOff *= 0.99f;
                }
            }
            else
            {
                levelSample = 0.0f;
            }

            ++currentPulse->index;
            return levelSample;
        }

        currentPulse->index = 0; // reset
        tailOff = 1.0f;
        std::rotate(musicMetre.pulseGroup.begin(), musicMetre.pulseGroup.begin() + 1, musicMetre.pulseGroup.end());

        return getLevelSample();
    }

    Music::Metre& musicMetre;

    // const unsigned int tableSize = 1 << 7;
    const unsigned int tableSize = 1 << 11;
    float level = 0.3f;

    Waveform waveform;
    AudioSampleBuffer soundTable;
    double soundFileSampleRate = 0.0;
    std::unique_ptr<WavetableOscillator> oscillator;

    bool stopped { true };
    float tailOff = 1.0;
};

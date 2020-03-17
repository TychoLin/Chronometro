#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include <list>

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
    enum class NoteValue
    {
        whole = 1,
        half = 2,
        quarter = 4,
        eighth = 8,
        tuplet = 12,
        sixteenth = 16
    };

    struct Pulse;

    struct PulseChangeBroadcaster : public ChangeBroadcaster
    {
        PulseChangeBroadcaster(Pulse* owner) : owner(owner)
        {
        }

        ~PulseChangeBroadcaster() override
        {
            removeAllChangeListeners();
        }

        Pulse* owner;
    };

    struct Pulse
    {
        Pulse()
            : index(0),
              hit(true),
              accent(0.0f),
              noteValue(NoteValue::quarter)
        {
        }

        Pulse(bool hit, float accent, NoteValue noteValue, float timeLength)
            : index(0), hit(hit), noteValue(noteValue), pulseSampleLength(timeLength)
        {
            this->accent = jlimit(0.0f, 1.0f, accent);
        }

        int index;
        bool hit;
        float accent;
        NoteValue noteValue;
        float pulseSampleLength;

        std::shared_ptr<PulseChangeBroadcaster> pulseChangeBroadcaster = std::make_shared<PulseChangeBroadcaster>(this);
    };

    class Metre : public Timer, public ChangeListener
    {
    public:
        using PulseIterator = std::list<std::unique_ptr<Pulse>>::iterator;

        Metre()
        {
        }

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

        void timerCallback() override
        {
            tapTimes.clear();
            stopTimer();
        }

        void init()
        {
            float note4th = convertToSampleLength(NoteValue::quarter);
            float note8th = note4th * 0.5f;
            float note16th = note4th * 0.25f;

            pulseGroup.push_back(std::make_unique<Pulse>(true, 0.0f, NoteValue::quarter, note4th));
            pulseGroup.push_back(std::make_unique<Pulse>(true, 0.0f, NoteValue::quarter, note4th));
            pulseGroup.push_back(std::make_unique<Pulse>(true, 0.0f, NoteValue::quarter, note4th));
            pulseGroup.push_back(std::make_unique<Pulse>(true, 0.0f, NoteValue::quarter, note4th));

            for (auto& pulse : pulseGroup)
                pulse->pulseChangeBroadcaster->addChangeListener(this);

            currentPulse = pulseGroup.begin();
        }

        void reset()
        {
            for (auto& pulse : pulseGroup)
            {
                pulse->index = 0;
                pulse->pulseSampleLength = convertToSampleLength(pulse->noteValue);
            }

            currentPulse = pulseGroup.begin();
        }

        void changeListenerCallback(ChangeBroadcaster* source) override
        {
            jassert(static_cast<PulseChangeBroadcaster*>(source) != nullptr);

            // update pulseSampleLength
            auto* pulseChangeBroadcaster = static_cast<PulseChangeBroadcaster*>(source);
            pulseChangeBroadcaster->owner->pulseSampleLength = convertToSampleLength(pulseChangeBroadcaster->owner->noteValue);
        }

        float convertToSampleLength(NoteValue noteValue)
        {
            return getSampleRatePerBeat() * (float) baseNoteValue / (float) noteValue;
        }

        PulseIterator insertPulse(PulseIterator pos)
        {
            auto pulsePtr = std::make_unique<Pulse>();
            pulsePtr->pulseSampleLength = convertToSampleLength(pulsePtr->noteValue);
            pulsePtr->pulseChangeBroadcaster->addChangeListener(this);
            auto newPulseIterator = pulseGroup.insert(pos, std::move(pulsePtr));

            if (newPulseIterator == pulseGroup.begin())
                currentPulse = newPulseIterator;

            return newPulseIterator;
        }

        PulseIterator erasePulse(PulseIterator pos)
        {
            return pulseGroup.erase(pos);
        }

        double audioDeviceSampleRate;

        std::list<std::unique_ptr<Pulse>> pulseGroup;
        PulseIterator currentPulse;

    private:
        float BPM = 120.0f;
        NoteValue baseNoteValue = NoteValue::quarter;
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
        musicMetre.init();
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
            stopped = false;
            musicMetre.reset();

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
        if (musicMetre.currentPulse == musicMetre.pulseGroup.end())
            return 0.0f;

        if ((*musicMetre.currentPulse)->index == 0)
            oscillator->scaleFrequency(1 + (*musicMetre.currentPulse)->accent);

        if ((*musicMetre.currentPulse)->index < (*musicMetre.currentPulse)->pulseSampleLength)
        {
            float levelSample;

            if ((*musicMetre.currentPulse)->hit && tailOff > 0.005f)
            {
                levelSample = oscillator->getNextSample() * level;

                if (waveform == Waveform::LP_Jam_Block & (*musicMetre.currentPulse)->index > (int) tableSize / (1 + (*musicMetre.currentPulse)->accent))
                    levelSample = 0.0f;

                if ((*musicMetre.currentPulse)->index > (*musicMetre.currentPulse)->pulseSampleLength * 0.3f)
                {
                    levelSample *= tailOff;
                    tailOff *= 0.99f;
                }
            }
            else
            {
                levelSample = 0.0f;
            }

            ++(*musicMetre.currentPulse)->index;
            return levelSample;
        }

        (*musicMetre.currentPulse)->index = 0; // reset
        tailOff = 1.0f;
        ++musicMetre.currentPulse;

        if (musicMetre.currentPulse == musicMetre.pulseGroup.end())
            musicMetre.currentPulse = musicMetre.pulseGroup.begin();

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

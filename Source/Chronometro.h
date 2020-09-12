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

    void setTableDelta(float soundFileSampleRate, float sampleRate)
    {
        tableDelta = soundFileSampleRate / sampleRate;
    }

    void scaleFrequency(float ratio)
    {
        currentIndex = 0.0f;
        tableDelta = baseTableDelta * ratio;
    }

    void rewindToHead()
    {
        currentIndex = 0.0f;
    }

    forcedinline float getNextSample() noexcept
    {
        auto currentSample = readLinearInterpolated();

        if ((currentIndex += tableDelta) > tableSize)
            currentIndex -= tableSize;

        return currentSample;
    }

    forcedinline float getNextWavetableSample() noexcept
    {
        auto currentSample = readLinearInterpolated();

        if ((currentIndex += tableDelta) > tableSize)
            return 0.0f;

        return currentSample;
    }

private:
    forcedinline float readLinearInterpolated() noexcept
    {
        auto index0 = (unsigned int) currentIndex;
        auto index1 = index0 + 1;

        auto frac = currentIndex - (float) index0;

        auto* table = wavetable.getReadPointer(0);
        auto value0 = table[index0];
        auto value1 = table[index1];

        return value0 + frac * (value1 - value0);
    }

    const AudioSampleBuffer& wavetable;
    const int tableSize;
    float currentIndex = 0.0f, baseTableDelta = 0.0f, tableDelta = 0.0f;
};

class BeatAudioSource;

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

    class Pulse;
    class PulseIterator;
    class Metre;

    using Beat = std::vector<std::unique_ptr<Pulse>>;

    class Pulse
    {
    public:
        Pulse(bool hit, float accent, NoteValue noteValue, float timeLength, Beat& owner)
            : index(0), hit(hit), noteValue(noteValue), sampleLength(timeLength), owner(owner)
        {
            this->accent = jlimit(0.0f, 1.0f, accent);
        }

        Pulse(const Pulse&) = delete;
        Pulse& operator=(const Pulse&) = delete;

        bool getHit() { return hit; }
        void setHit(bool hitToUse) { hit = hitToUse; }
        float getAccent() { return accent; }
        void setAccent(float accentToUse) { accent = accentToUse; }
        NoteValue getNoteValue() { return noteValue; }
        void setNoteValue(NoteValue noteValueToUse)
        {
            for (auto& pulse : owner)
            {
                auto localSampleLength = Metre::convertToSampleLength(noteValueToUse);
                pulse->noteValue = noteValueToUse;
                pulse->sampleLength = localSampleLength;
            }
        }

        friend class PulseIterator;
        friend class Metre;
        friend class ::BeatAudioSource;

    private:
        int index;
        bool hit;
        float accent;
        NoteValue noteValue;
        float sampleLength;

        Beat& owner;
    };

    class PulseIterator
    {
    public:
        PulseIterator() = default;

        PulseIterator(std::list<Beat>::iterator bp, Beat::iterator pp) : beatPos(bp), pulsePos(pp) {}

        Pulse& operator*() const { return **pulsePos; }

        PulseIterator& operator++()
        {
            if (doesPulseIteratorLock.exchange(true, std::memory_order_release) == false
                && doesNoteButtonLock.load(std::memory_order_acquire) == false)
            {
                // data race when beatEnd is before pulsePos.
                // At that moment, pulsePos will access wrong memory location.
                auto beatEnd = (*beatPos).begin() + (int) (*pulsePos)->noteValue / (int) Metre::baseNoteValue;

                ++pulsePos;

                if (pulsePos >= beatEnd)
                {
                    ++beatPos;
                    pulsePos = (*beatPos).begin();
                }
            }

            doesPulseIteratorLock.store(false, std::memory_order_release);

            return *this;
        }

        bool operator==(const PulseIterator& other) const
        {
            return beatPos == other.beatPos && pulsePos == other.pulsePos;
        }

        bool operator!=(const PulseIterator& other) const
        {
            return !(*this == other);
        }

        inline static std::atomic<bool> doesPulseIteratorLock { false };
        inline static std::atomic<bool> doesNoteButtonLock { false };

    private:
        std::list<Beat>::iterator beatPos;
        Beat::iterator pulsePos;
    };

    class Metre : public Timer
    {
    public:
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

        static float getIntervalPerBeat()
        {
            return 60.0f / BPM;
        }

        static double getSampleRatePerBeat()
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

            beatList.push_back(Beat {});
            beatList.push_back(Beat {});
            beatList.push_back(Beat {});
            beatList.push_back(Beat {});
            beatList.push_back(Beat {}); // not used

            auto last = --beatList.end();

            for (auto it = beatList.begin(); it != last; ++it)
            {
                it->push_back(std::make_unique<Pulse>(true, 0.0f, NoteValue::quarter, note4th, *it));
                it->push_back(std::make_unique<Pulse>(true, 0.0f, NoteValue::quarter, note4th, *it));
                it->push_back(std::make_unique<Pulse>(true, 0.0f, NoteValue::quarter, note4th, *it));
                it->push_back(std::make_unique<Pulse>(true, 0.0f, NoteValue::quarter, note4th, *it));
            }

            currentPulse = begin();
        }

        void update()
        {
            for (auto& pulse : *this)
            {
                pulse.index = 0;
                pulse.sampleLength = convertToSampleLength(pulse.noteValue);
            }

            currentPulse = begin();
        }

        static float convertToSampleLength(NoteValue noteValue)
        {
            return getSampleRatePerBeat() * (float) baseNoteValue / (float) noteValue;
        }

        PulseIterator begin()
        {
            return PulseIterator(beatList.begin(), (*beatList.begin()).begin());
        }

        PulseIterator end()
        {
            auto last = beatList.end();
            --last;
            return PulseIterator(last, (*last).end());
        }

        inline static double audioDeviceSampleRate { 44100.0 };

        std::list<Beat> beatList;
        PulseIterator currentPulse;

        inline static NoteValue baseNoteValue = NoteValue::quarter;

    private:
        inline static float BPM { 120.0f };
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

        // auto frequency = 1760.0f;
        // oscillator->setFrequency((float) frequency, (float) sampleRate);

        oscillator->setTableDelta((float) soundFileSampleRate, (float) sampleRate);

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
            musicMetre.update();

            sendChangeMessage();
        }
    }

    void stop()
    {
        if (!stopped)
        {
            stopped = true;
            tailOff = 1.0;
            oscillator->rewindToHead();

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
                String relativeFilePath = "Resources/LP_Jam_Block.ogg";

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
        if (musicMetre.currentPulse == musicMetre.end())
            return 0.0f;

        if ((*musicMetre.currentPulse).index < (*musicMetre.currentPulse).sampleLength)
        {
            float levelSample;

            if ((*musicMetre.currentPulse).hit && tailOff > 0.005f)
            {
                if (waveform == Waveform::LP_Jam_Block)
                    levelSample = oscillator->getNextWavetableSample();
                else
                    levelSample = oscillator->getNextSample();

                if ((*musicMetre.currentPulse).index > (*musicMetre.currentPulse).sampleLength * 0.3f)
                {
                    levelSample *= tailOff;
                    tailOff *= 0.99f;
                }
            }
            else
            {
                levelSample = 0.0f;
            }

            ++(*musicMetre.currentPulse).index;
            auto accent = (*musicMetre.currentPulse).accent;
            return (accent > 0.0f) ? std::tanh((1 + std::log(10 * accent + 1)) * levelSample) : levelSample;
        }

        (*musicMetre.currentPulse).index = 0; // reset
        oscillator->rewindToHead();
        tailOff = 1.0f;
        ++musicMetre.currentPulse;

        if (musicMetre.currentPulse == musicMetre.end())
            musicMetre.currentPulse = musicMetre.begin();

        return getLevelSample();
    }

    Music::Metre& musicMetre;

    // const unsigned int tableSize = 1 << 7;
    const unsigned int tableSize = 1 << 11;

    Waveform waveform;
    AudioSampleBuffer soundTable;
    double soundFileSampleRate = 0.0;
    std::unique_ptr<WavetableOscillator> oscillator;

    bool stopped { true };
    float tailOff = 1.0;
};

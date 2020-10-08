#pragma once

class Music
{
public:
    enum class NoteValue
    {
        whole = 1,
        half = 2,
        quarter = 4,
        eighth = 8,
        triplet = 12,
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
            auto localSampleLength = Metre::convertToSampleLength(noteValueToUse);

            for (auto& pulse : owner)
            {
                pulse->noteValue = noteValueToUse;
                pulse->sampleLength = localSampleLength;
            }
        }

        friend class PulseIterator;
        friend class Metre;

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

            tailOff = 1.0f;
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

        float getPulseSample(AudioProcessor& audioProcessor)
        {
            if (currentPulse == end())
                return 0.0f;

            if ((*currentPulse).index < (*currentPulse).sampleLength)
            {
                float pulseSample = 1.0f;

                if ((*currentPulse).hit && tailOff > 0.001f)
                {
                    if ((*currentPulse).index > 2048)
                    {
                        pulseSample *= tailOff;
                        tailOff *= 0.99f;
                    }
                }
                else
                {
                    pulseSample = 0.0f;
                }

                ++(*currentPulse).index;
                return pulseSample;
            }

            (*currentPulse).index = 0; // reset
            audioProcessor.reset();
            tailOff = 1.0f;
            ++currentPulse;

            if (currentPulse == end())
                currentPulse = begin();

            return getPulseSample(audioProcessor);
        }

        inline static double audioDeviceSampleRate { 44100.0 };

        std::list<Beat> beatList;
        PulseIterator currentPulse;

        inline static NoteValue baseNoteValue = NoteValue::quarter;

    private:
        inline static float BPM { 120.0f };
        std::vector<double> tapTimes;

        float tailOff = 1.0f;
    };

private:
    Music() = delete;
};

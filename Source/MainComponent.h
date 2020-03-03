/*
  ==============================================================================

    This file was auto-generated!

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include <algorithm>

class AppLookAndFeel : public LookAndFeel_V4
{
public:
    AppLookAndFeel()
    {
        auto colourScheme = getDarkColourScheme();
        colourScheme.setUIColour(ColourScheme::defaultText, Colours::teal);
        colourScheme.setUIColour(ColourScheme::highlightedText, Colours::teal);
        colourScheme.setUIColour(ColourScheme::widgetBackground, Colours::mediumaquamarine);
        setColourScheme(colourScheme);
        setColour(Label::backgroundColourId, Colours::mediumaquamarine);
    }

    void drawButtonBackground(
        Graphics& g,
        Button& button,
        const Colour& backgroundColour,
        bool isMouseOverButton,
        bool isButtonDown) override
    {
        auto buttonArea = button.getLocalBounds();

        g.setColour(backgroundColour);
        g.fillRect(buttonArea);

        if (isMouseOverButton)
        {
            g.setColour(backgroundColour.brighter());
            g.fillRect(buttonArea);
        }
    }
};

class CircleBeatComponent : public Component
{
public:
    CircleBeatComponent(bool fill) : fill(fill) {}

    void paint(Graphics& g) override
    {
        g.setColour(Colours::whitesmoke);

        auto lineThickness = 1.0f;

        if (fill)
        {
            g.fillEllipse(lineThickness * 0.5f,
                          lineThickness * 0.5f,
                          getWidth() - lineThickness * 2,
                          getHeight() - lineThickness * 2);
        }
        else
        {
            g.drawEllipse(lineThickness * 0.5f,
                          lineThickness * 0.5f,
                          getWidth() - lineThickness * 2,
                          getHeight() - lineThickness * 2,
                          lineThickness);
        }
    }

    bool fill;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CircleBeatComponent)
};

class VisualBeatComponent : public Component, public Timer
{
public:
    VisualBeatComponent() : leftCircleBeat(false), rightCircleBeat(true)
    {
        addAndMakeVisible(&leftCircleBeat);
        addAndMakeVisible(&rightCircleBeat);
    }

    void paint(Graphics& g) override
    {
        g.fillAll(Colours::mediumvioletred);
    }

    void resized() override
    {
        int margin = 10;
        int diameter = (getWidth() - margin - 10 * 2) / 2;
        leftCircleBeat.setBounds(10, 10, diameter, diameter);
        rightCircleBeat.setBounds(10 + diameter + margin, 10, diameter, diameter);
    }

    void mouseDown(const MouseEvent& event) override
    {
        stopTimer();
    }

private:
    void timerCallback() override
    {
        repaint();
        leftCircleBeat.fill = !leftCircleBeat.fill;
        rightCircleBeat.fill = !rightCircleBeat.fill;
    }

    CircleBeatComponent leftCircleBeat;
    CircleBeatComponent rightCircleBeat;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VisualBeatComponent)
};

class WavetableOscillator
{
public:
    WavetableOscillator(const AudioSampleBuffer& wavetableToUse)
        : wavetable(wavetableToUse),
          tableSize(wavetable.getNumSamples() - 1)
    {
        jassert(wavetable.getNumChannels() == 1);
    }

    void setFrequency(float frequency, float sampleRate)
    {
        auto tableSizeOverSampleRate = tableSize / sampleRate;
        tableDelta = frequency * tableSizeOverSampleRate;
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
    float currentIndex = 0.0f, tableDelta = 0.0f;
};

class Music
{
public:
    struct Pulse
    {
        Pulse(bool hit, float timeLength) : index(0), hit(hit), pulseSampleRate(timeLength) {}

        int index;
        bool hit;
        float pulseSampleRate;
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
                Pulse(true, note16th),
                Pulse(false, note16th),
                Pulse(true, note16th),
                Pulse(true, note16th),
                Pulse(false, note16th),
                Pulse(true, note16th),
                Pulse(true, note16th),
                Pulse(false, note16th),
                Pulse(false, note8th),
                Pulse(true, note8th),
                Pulse(false, note8th),
                Pulse(true, note8th)
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
        createWavetable();
    }

    void prepareToPlay(int /*samplesPerBlockExpected*/, double sampleRate) override
    {
        oscillator.reset(new WavetableOscillator(sineTable));
        auto frequency = 440.0f;

        oscillator->setFrequency((float) frequency, (float) sampleRate);

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

private:
    void createWavetable()
    {
        sineTable.setSize(1, (int) tableSize + 1);
        auto* samples = sineTable.getWritePointer(0);

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

    double getLevelSample()
    {
        auto currentPulse = musicMetre.pulseGroup.begin();

        if (currentPulse == musicMetre.pulseGroup.end())
            return 0.0;

        if (currentPulse->index < currentPulse->pulseSampleRate)
        {
            double levelSample;

            if (currentPulse->hit && tailOff > 0.005)
            {
                if (currentPulse->index < currentPulse->pulseSampleRate * 0.3)
                {
                    levelSample = oscillator->getNextSample() * level;
                }
                else
                {
                    levelSample = oscillator->getNextSample() * level * tailOff;
                    tailOff *= 0.99;
                }
            }
            else
            {
                levelSample = 0.0;
            }

            ++currentPulse->index;
            return levelSample;
        }

        currentPulse->index = 0; // reset
        tailOff = 1.0;
        std::rotate(musicMetre.pulseGroup.begin(), musicMetre.pulseGroup.begin() + 1, musicMetre.pulseGroup.end());
        return getLevelSample();
    }

    Music::Metre& musicMetre;

    const unsigned int tableSize = 1 << 7;
    float level = 0.25f;

    AudioSampleBuffer sineTable;
    std::unique_ptr<WavetableOscillator> oscillator;

    bool stopped { true };
    double tailOff = 1.0;
};

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent : public AudioAppComponent, public ChangeListener
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent();

    //==============================================================================
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==============================================================================
    void paint(Graphics& g) override;
    void resized() override;

    void changeListenerCallback(ChangeBroadcaster* source) override;

private:
    //==============================================================================
    // Your private member variables go here...
    enum TransportState
    {
        Stopped,
        Starting,
        Playing,
        Stopping
    };

    void changeState(TransportState newState)
    {
        if (state != newState)
        {
            state = newState;

            switch (state)
            {
                case Stopped:
                    break;

                case Starting:
                    beatAudioSource.start();
                    break;

                case Playing:
                    break;

                case Stopping:
                    beatAudioSource.stop();
                    break;
            }
        }
    }

    void playButtonClicked()
    {
        if (state != Playing)
        {
            changeState(Starting);
            // visualBeatRegion.startTimer(static_cast<int>(musicMetre.getIntervalPerBeat() * 1000));
        }
        else
            startButton.setToggleState(true, dontSendNotification);
    }

    void stopButtonClicked()
    {
        if (state != Stopped)
        {
            changeState(Stopping);
            // visualBeatRegion.stopTimer();
        }
    }

    class BPMLabel : public Label
    {
    public:
        void setBPM(float BPM)
        {
            setText(String::toDecimalStringWithSignificantFigures(BPM, 5), dontSendNotification);
        }
    };

    AppLookAndFeel appLookAndFeel;

    TextButton tapButton;
    BPMLabel BPMLabel;
    TextButton startButton;
    TextButton stopButton;
    VisualBeatComponent visualBeatRegion;

    BeatAudioSource beatAudioSource;
    Music::Metre musicMetre;

    TransportState state;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

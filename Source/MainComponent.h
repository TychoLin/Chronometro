/*
  ==============================================================================

    This file was auto-generated!

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "Chronometro.h"

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

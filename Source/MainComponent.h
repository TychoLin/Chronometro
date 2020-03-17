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
        colourScheme.setUIColour(ColourScheme::outline, Colours::transparentBlack);
        setColourScheme(colourScheme);
        setColour(Label::backgroundColourId, Colours::mediumaquamarine);
        setColour(TextButton::buttonOnColourId, Colours::lemonchiffon);
    }

    Font getTextButtonFont(TextButton&, int buttonHeight) override
    {
        return { buttonHeight * 0.6f };
    }

    Font getLabelFont(Label& label) override
    {
        return { label.getHeight() * 0.6f };
    }
};

class CircleBeatComponent : public Component
{
public:
    CircleBeatComponent(bool fill) : fill(fill) {}

    void paint(Graphics& g) override
    {
        g.setColour(Colours::teal);

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
        g.fillAll(Colours::mediumaquamarine);
    }

    void resized() override
    {
        int diameter = getHeight() * 0.7;
        int marginTop = getHeight() * 0.15;
        int marginLeft = (getWidth() - 2 * diameter) / 3;
        leftCircleBeat.setBounds(marginLeft, marginTop, diameter, diameter);
        rightCircleBeat.setBounds(diameter + 2 * marginLeft, marginTop, diameter, diameter);
    }

    void mouseDown(const MouseEvent& event) override
    {
    }

private:
    void timerCallback() override
    {
        repaint();
        std::swap(leftCircleBeat.fill, rightCircleBeat.fill);
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
            headerPanel.visualBeatRegion.startTimer(static_cast<int>(musicMetre.getIntervalPerBeat() * 1000));
            changeState(Starting);
        }
        else
            headerPanel.startButton.setToggleState(true, dontSendNotification);
    }

    void stopButtonClicked()
    {
        if (state != Stopped)
        {
            headerPanel.visualBeatRegion.stopTimer();
            changeState(Stopping);
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

    class NoteButton : public TextButton
    {
    public:
        NoteButton(const Music::Metre::PulseIterator it) : pulseIterator(it)
        {
            state = noteValueVector.begin();
            while (state != noteValueVector.end() && *state != (*it)->noteValue)
                ++state;

            setButtonText(String((int) *state));
            onClick = [this] {
                if (++state == noteValueVector.end())
                    state = noteValueVector.begin();

                setButtonText(String((int) *state));
                (*pulseIterator)->noteValue = *state;
                (*pulseIterator)->pulseChangeBroadcaster->sendChangeMessage();
            };
        }

        const Music::Metre::PulseIterator pulseIterator;

    private:
        std::vector<Music::NoteValue>::iterator state;
        std::vector<Music::NoteValue> noteValueVector {
            Music::NoteValue::quarter,
            Music::NoteValue::eighth,
            Music::NoteValue::tuplet,
            Music::NoteValue::sixteenth
        };
    };

    struct MetreComponent : public Component
    {
        MetreComponent(const Music::Metre::PulseIterator it)
        {
            noteButton.reset(new NoteButton(it));
            addAndMakeVisible(noteButton.get());

            addButton.setButtonText("+");
            addButton.onClick = [this] {
                static_cast<BodyPanel*>(getParentComponent())->metreAddButtonClicked(noteButton->pulseIterator);
            };
            addAndMakeVisible(&addButton);

            deleteButton.setButtonText("-");
            deleteButton.onClick = [this] {
                static_cast<BodyPanel*>(getParentComponent())->metreDeleteButtonClicked(noteButton->pulseIterator);
            };
            addAndMakeVisible(&deleteButton);

            hitButton.setButtonText("h");
            hitButton.setToggleState((*noteButton->pulseIterator)->hit, dontSendNotification);
            hitButton.onClick = [this] {
                hitButton.setToggleState(!hitButton.getToggleState(), dontSendNotification);
                (*noteButton->pulseIterator)->hit = hitButton.getToggleState();
            };
            addAndMakeVisible(&hitButton);

            accentButton.setButtonText(">");
            accentButton.onClick = [this] {
                accentButton.setToggleState(!accentButton.getToggleState(), dontSendNotification);
                (*noteButton->pulseIterator)->accent = accentButton.getToggleState() ? 0.3f : 0.0f;
            };
            addAndMakeVisible(&accentButton);
        }

        void paint(Graphics& g) override
        {
        }

        void resized() override
        {
            FlexBox fbGroup;
            FlexBox fb;

            fbGroup.items.add(FlexItem(addButton).withFlex(1));
            fbGroup.items.add(FlexItem(deleteButton).withFlex(1));
            fbGroup.items.add(FlexItem(hitButton).withFlex(1));
            fbGroup.items.add(FlexItem(accentButton).withFlex(1));

            fb.flexWrap = FlexBox::Wrap::wrap;
            fb.justifyContent = FlexBox::JustifyContent::flexStart;
            fb.alignContent = FlexBox::AlignContent::flexStart;

            fb.items.add(FlexItem(fbGroup).withMinWidth(getWidth()).withMinHeight(getHeight() / 3.0f));
            fb.items.add(FlexItem(*noteButton).withMinWidth(getWidth()).withMinHeight(getHeight() * 2.0f / 3.0f));

            fb.performLayout(getLocalBounds().toFloat());
        }

        std::unique_ptr<NoteButton> noteButton;
        TextButton addButton;
        TextButton deleteButton;
        TextButton hitButton;
        TextButton accentButton;
    };

    struct HeaderPanel : public Component
    {
        HeaderPanel(Music::Metre& metre) : musicMetre(metre)
        {
            tapButton.setButtonText("Tap");
            tapButton.onClick = [this] {
                musicMetre.setTapBPM();
                BPMLabel.setBPM(musicMetre.getBPM());
            };
            addAndMakeVisible(&tapButton);

            BPMLabel.setEditable(true);
            BPMLabel.setBPM(musicMetre.getBPM());
            BPMLabel.onTextChange = [this] {
                auto bpm = BPMLabel.getText().getFloatValue();
                musicMetre.setBPM(bpm);
                BPMLabel.setBPM(musicMetre.getBPM());
            };
            addAndMakeVisible(&BPMLabel);

            startButton.setButtonText("Start");
            startButton.setClickingTogglesState(true);
            startButton.onClick = [this] { static_cast<MainComponent*>(getParentComponent())->playButtonClicked(); };
            addAndMakeVisible(&startButton);

            stopButton.setButtonText("Stop");
            stopButton.onClick = [this] {
                startButton.setToggleState(false, dontSendNotification);
                static_cast<MainComponent*>(getParentComponent())->stopButtonClicked();
            };
            addAndMakeVisible(&stopButton);

            addAndMakeVisible(&visualBeatRegion);
        }

        void paint(Graphics& g) override
        {
        }

        void resized() override
        {
            auto isPortrait = getHeight() > getWidth();

            FlexBox fb1stGroup;
            FlexBox fb2ndGroup;
            FlexBox fbContainer;
            float margin = 8.0f;

            fb1stGroup.flexDirection = isPortrait ? FlexBox::Direction::column : FlexBox::Direction::row;

            fb1stGroup.items.add(FlexItem(tapButton).withFlex(1));
            fb1stGroup.items.add(FlexItem(startButton).withFlex(1));
            fb1stGroup.items.add(FlexItem(stopButton).withFlex(1));

            fb2ndGroup.flexWrap = FlexBox::Wrap::wrap;
            fb2ndGroup.justifyContent = FlexBox::JustifyContent::flexStart;
            fb2ndGroup.alignContent = FlexBox::AlignContent::flexStart;

            if (isPortrait)
            {
                FlexItem::Margin BPMLabelMargin;
                BPMLabelMargin.bottom = margin;
                fb2ndGroup.items.add(FlexItem(BPMLabel).withMinWidth(getWidth()).withMinHeight(getWidth() * 0.5f).withMargin(BPMLabelMargin));
                fb2ndGroup.items.add(FlexItem(visualBeatRegion).withMinWidth(getWidth()).withMinHeight(getWidth() * 0.5f));
            }
            else
            {
                FlexItem::Margin BPMLabelMargin;
                BPMLabelMargin.right = margin;
                fb2ndGroup.items.add(FlexItem(BPMLabel).withMinWidth(getHeight()).withMinHeight(getHeight() * 0.5f).withMargin(BPMLabelMargin));
                fb2ndGroup.items.add(FlexItem(visualBeatRegion).withMinWidth(getHeight()).withMinHeight(getHeight() * 0.5f));
            }

            fbContainer.flexDirection = FlexBox::Direction::column;

            FlexItem::Margin fb1stGroupMargin;
            fb1stGroupMargin.bottom = margin;
            fbContainer.items.addArray({ FlexItem(fb1stGroup).withFlex(1).withMargin(fb1stGroupMargin),
                                         FlexItem(fb2ndGroup).withFlex(1) });
            fbContainer.performLayout(getLocalBounds().toFloat());
        }

        Music::Metre& musicMetre;

        TextButton tapButton;
        BPMLabel BPMLabel;
        TextButton startButton;
        TextButton stopButton;
        VisualBeatComponent visualBeatRegion;
    };

    struct BodyPanel : public Component
    {
        BodyPanel(Music::Metre& metre) : musicMetre(metre)
        {
        }

        void paint(Graphics& g) override
        {
        }

        void resized() override
        {
            FlexBox fb;

            fb.flexWrap = FlexBox::Wrap::wrap;
            fb.justifyContent = FlexBox::JustifyContent::flexStart;
            fb.alignContent = FlexBox::AlignContent::flexStart;

            for (auto& metrePtr : metreList)
                fb.items.add(FlexItem(*metrePtr).withMinWidth(getWidth() / 4.0f).withMinHeight(getWidth() / 8.0f));

            fb.performLayout(getLocalBounds().toFloat());
        }

        void init()
        {
            for (auto it = musicMetre.pulseGroup.begin(); it != musicMetre.pulseGroup.end(); ++it)
                addAndMakeVisible(**metreList.insert(metreList.end(), std::make_unique<MetreComponent>(it)));

            resized();
        }

        void metreAddButtonClicked(const Music::Metre::PulseIterator pulseIterator)
        {
            auto metrePtr = std::make_unique<MetreComponent>(musicMetre.insertPulse(pulseIterator));

            auto metrePos = metreList.begin();
            while (metrePos != metreList.end() && (*metrePos)->noteButton->pulseIterator != pulseIterator)
                ++metrePos;

            addAndMakeVisible(**metreList.insert(metrePos, std::move(metrePtr)));
            resized();
        }

        void metreDeleteButtonClicked(const Music::Metre::PulseIterator pulseIterator)
        {
            if (!static_cast<MainComponent*>(getParentComponent())->beatAudioSource.isPlaying() && musicMetre.currentPulse != pulseIterator)
            {
                auto metrePos = metreList.begin();
                while (metrePos != metreList.end() && (*metrePos)->noteButton->pulseIterator != pulseIterator)
                    ++metrePos;

                metreList.erase(metrePos);
                musicMetre.erasePulse(pulseIterator);
                resized();
            }
        }

        Music::Metre& musicMetre;

        std::list<std::unique_ptr<MetreComponent>> metreList;
    };

    AppLookAndFeel appLookAndFeel;

    Music::Metre musicMetre;
    BeatAudioSource beatAudioSource;

    TransportState state;

    HeaderPanel headerPanel;
    BodyPanel bodyPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

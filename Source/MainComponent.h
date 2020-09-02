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
        colourScheme.setUIColour(ColourScheme::defaultText, Colours::lightseagreen);
        colourScheme.setUIColour(ColourScheme::highlightedText, Colours::lightseagreen);
        colourScheme.setUIColour(ColourScheme::outline, Colours::transparentBlack);
        setColourScheme(colourScheme);
        setColour(Label::textColourId, Colours::lightseagreen);
        setColour(TextButton::buttonColourId, Colours::teal);
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
        g.setColour(Colours::lightseagreen);

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
        g.fillAll(Colours::teal);
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

            for (auto& beatPtr : bodyPanel.metreListPanel.beatList)
                beatPtr->pulseList.front()->noteButton->setVisible(false);
        }
    }

    void stopButtonClicked()
    {
        if (state != Stopped)
        {
            headerPanel.visualBeatRegion.stopTimer();
            changeState(Stopping);

            for (auto& beatPtr : bodyPanel.metreListPanel.beatList)
                beatPtr->pulseList.front()->noteButton->setVisible(true);
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
        NoteButton(Music::Pulse& pulseToUse) : pulse(pulseToUse)
        {
            state = noteValueVector.begin();
            while (state != noteValueVector.end() && *state != pulse.noteValue)
                ++state;

            setButtonText(String((int) *state));
            onClick = [this] {
                if (++state == noteValueVector.end())
                    state = noteValueVector.begin();

                setButtonText(String((int) *state));
                pulse.noteValue = *state;
                pulse.pulseChangeBroadcaster->sendChangeMessage();
                static_cast<BeatComponent*>(getParentComponent()->getParentComponent())->resized();
            };
        }

        Music::Pulse& pulse;

    private:
        std::vector<Music::NoteValue>::iterator state;
        std::vector<Music::NoteValue> noteValueVector {
            Music::NoteValue::quarter,
            Music::NoteValue::eighth,
            Music::NoteValue::tuplet,
            Music::NoteValue::sixteenth
        };
    };

    struct PulseComponent : public Component
    {
        PulseComponent(Music::Pulse& pulseToUse, bool shouldUseNoteButton = false) : pulse(pulseToUse)
        {
            if (shouldUseNoteButton)
            {
                noteButton.reset(new NoteButton(pulse));
                addAndMakeVisible(noteButton.get());
            }

            hitButton.setToggleState(pulse.hit, dontSendNotification);
            hitButton.onClick = [this] {
                hitButton.setToggleState(!hitButton.getToggleState(), dontSendNotification);
                pulse.hit = hitButton.getToggleState();
            };
            addAndMakeVisible(&hitButton);

            accentButton.setButtonText(">");
            accentButton.onClick = [this] {
                accentButton.setToggleState(!accentButton.getToggleState(), dontSendNotification);
                pulse.accent = accentButton.getToggleState() ? 0.5f : 0.0f;
            };
            addAndMakeVisible(&accentButton);
        }

        void resized() override
        {
            FlexBox fbGroup;
            FlexBox fb;

            fbGroup.items.add(FlexItem(*noteButton).withFlex(1));
            fbGroup.items.add(FlexItem(accentButton).withFlex(1));

            fb.flexDirection = FlexBox::Direction::column;

            fb.items.add(FlexItem(fbGroup).withMinWidth(getWidth()).withMinHeight(getHeight() / 3.0f));
            fb.items.add(FlexItem(hitButton).withMinWidth(getWidth()).withMinHeight(getHeight() * 2.0f / 3.0f));

            fb.performLayout(getLocalBounds().toFloat());
        }

        Music::Pulse& pulse;

        std::unique_ptr<NoteButton> noteButton;
        TextButton hitButton;
        TextButton accentButton;
    };

    struct BeatComponent : public Component
    {
        BeatComponent(Music::Beat& beatToUse) : beat(beatToUse)
        {
            addAndMakeVisible(**pulseList.insert(pulseList.end(), std::make_unique<PulseComponent>(*beat[0], true)));
            addChildComponent(**pulseList.insert(pulseList.end(), std::make_unique<PulseComponent>(*beat[1])));
            addChildComponent(**pulseList.insert(pulseList.end(), std::make_unique<PulseComponent>(*beat[2])));
            addChildComponent(**pulseList.insert(pulseList.end(), std::make_unique<PulseComponent>(*beat[3])));
        }

        void resized() override
        {
            FlexBox fb;

            fb.flexDirection = FlexBox::Direction::row;

            int n = (int) beat[0]->noteValue / (int) Music::Metre::baseNoteValue;
            auto last = pulseList.begin();

            while (n-- > 0)
                ++last;

            for (auto it = pulseList.begin(); it != last; ++it)
            {
                (**it).setVisible(true);
                fb.items.add(FlexItem(**it).withMinWidth(86.0f).withMinHeight(86.0f * 2.0f / 3.0f).withFlex(1));
            }

            for (auto it = last; it != pulseList.end(); ++it)
                (**it).setVisible(false);

            fb.performLayout(getLocalBounds().toFloat());
        }

        Music::Beat& beat;

        std::list<std::unique_ptr<PulseComponent>> pulseList;
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
            startButton.onClick = [this] {
                if (startButton.getToggleState())
                {
                    static_cast<MainComponent*>(getParentComponent())->playButtonClicked();
                    startButton.setButtonText("Stop");
                }
                else
                {
                    static_cast<MainComponent*>(getParentComponent())->stopButtonClicked();
                    startButton.setButtonText("Start");
                }
            };
            addAndMakeVisible(&startButton);

            settingButton.setButtonText("Setting");
            settingButton.setClickingTogglesState(true);
            settingButton.onClick = [this] {
                auto* mainComponent = static_cast<MainComponent*>(getParentComponent());

                if (settingButton.getToggleState())
                {
                    mainComponent->bodyPanel.setCurrentPanel(&mainComponent->bodyPanel.settingPanel);
                }
                else
                {
                    mainComponent->bodyPanel.setCurrentPanel(&mainComponent->bodyPanel.metreListPanel);
                }
            };
            addAndMakeVisible(&settingButton);

            addAndMakeVisible(&visualBeatRegion);
        }

        void resized() override
        {
            auto isPortrait = getHeight() > getWidth();

            FlexBox fb1stGroup;
            FlexBox fb2ndGroup;
            FlexBox fbContainer;
            float fb1stGroupFelxGrow, fb2ndGroupFlexGrow;

            if (isPortrait)
            {
                fb1stGroup.flexDirection = FlexBox::Direction::column;
                fb1stGroupFelxGrow = 3.0f;

                fb1stGroup.items.add(FlexItem(tapButton).withMinWidth(154.0f).withMinHeight(66.0f).withFlex(1));
                fb1stGroup.items.add(FlexItem(startButton).withMinWidth(154.0f).withMinHeight(66.0f).withFlex(1));
                fb1stGroup.items.add(FlexItem(settingButton).withMinWidth(154.0f).withMinHeight(66.0f).withFlex(1));

                fb2ndGroup.flexDirection = FlexBox::Direction::column;
                fb2ndGroupFlexGrow = 2.0f;

                FlexItem::Margin BPMLabelMargin;
                BPMLabelMargin.bottom = 1.0f;
                fb2ndGroup.items.add(FlexItem(BPMLabel).withMinWidth(154.0f).withMinHeight(66.0f).withMargin(BPMLabelMargin).withFlex(1));
                fb2ndGroup.items.add(FlexItem(visualBeatRegion).withMinWidth(154.0f).withMinHeight(66.0f).withFlex(1));
            }
            else
            {
                fb1stGroup.flexDirection = FlexBox::Direction::row;
                fb1stGroupFelxGrow = 1.0f;

                fb1stGroup.items.add(FlexItem(tapButton).withMinWidth(114.0f).withMinHeight(56.0f).withFlex(1));
                fb1stGroup.items.add(FlexItem(startButton).withMinWidth(114.0f).withMinHeight(56.0f).withFlex(1));
                fb1stGroup.items.add(FlexItem(settingButton).withMinWidth(114.0f).withMinHeight(56.0f).withFlex(1));

                fb2ndGroup.flexDirection = FlexBox::Direction::row;
                fb2ndGroupFlexGrow = 1.0f;

                FlexItem::Margin BPMLabelMargin;
                BPMLabelMargin.right = 1.0f;
                fb2ndGroup.items.add(FlexItem(BPMLabel).withMinWidth(114.0f).withMinHeight(56.0f).withMargin(BPMLabelMargin).withFlex(1));
                fb2ndGroup.items.add(FlexItem(visualBeatRegion).withMinWidth(114.0f).withMinHeight(56.0f).withFlex(1));
            }

            fbContainer.flexDirection = FlexBox::Direction::column;

            FlexItem::Margin fb1stGroupMargin;
            fb1stGroupMargin.bottom = 8.0f;
            fbContainer.items.addArray({ FlexItem(fb1stGroup).withFlex(fb1stGroupFelxGrow).withMargin(fb1stGroupMargin),
                                         FlexItem(fb2ndGroup).withFlex(fb2ndGroupFlexGrow) });
            fbContainer.performLayout(getLocalBounds().toFloat());
        }

        Music::Metre& musicMetre;

        TextButton tapButton;
        BPMLabel BPMLabel;
        TextButton startButton;
        TextButton settingButton;
        VisualBeatComponent visualBeatRegion;
    };

    struct MetreListPanel : public Component
    {
        MetreListPanel(Music::Metre& metre) : musicMetre(metre)
        {
        }

        void resized() override
        {
            FlexBox fb;

            fb.flexWrap = FlexBox::Wrap::wrap;
            fb.justifyContent = FlexBox::JustifyContent::flexStart;
            fb.alignContent = FlexBox::AlignContent::flexStart;

            FlexItem::Margin beatMargin;
            beatMargin.bottom = 8.0f;

            for (auto& metrePtr : beatList)
                fb.items.add(FlexItem(*metrePtr)
                                 .withWidth(getWidth())
                                 .withHeight(getWidth() / 6.0f)
                                 .withMinHeight(86.0f * 2.0f / 3.0f)
                                 .withMargin(beatMargin));

            fb.performLayout(getLocalBounds().toFloat());
        }

        void init()
        {
            auto last = --musicMetre.beatList.end();

            for (auto it = musicMetre.beatList.begin(); it != last; ++it)
                addAndMakeVisible(**beatList.insert(beatList.end(), std::make_unique<BeatComponent>(*it)));

            resized();
        }

        Music::Metre& musicMetre;

        std::list<std::unique_ptr<BeatComponent>> beatList;
    };

    struct SettingPanel : public Component
    {
        SettingPanel()
        {
            addAndMakeVisible(&wrapDecibelSlider);
        }

        void resized() override
        {
            auto isPortrait = getHeight() > getWidth();

            FlexBox fb;

            fb.flexDirection = FlexBox::Direction::column;

            fb.items.add(FlexItem(wrapDecibelSlider).withFlex(0, 1, isPortrait ? getHeight() / 10.0f : getHeight() / 5.0f));

            fb.performLayout(getLocalBounds().toFloat());
        }

        class DecibelSlider : public Slider
        {
        public:
            DecibelSlider() {}

            double getValueFromText(const String& text) override
            {
                auto minusInfinitydB = -100.0;

                auto decibelText = text.upToFirstOccurrenceOf("dB", false, false).trim();

                return decibelText.equalsIgnoreCase("-INF") ? minusInfinitydB
                                                            : decibelText.getDoubleValue();
            }

            String getTextFromValue(double value) override { return Decibels::toString(value); }

        private:
            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DecibelSlider)
        };

        struct WrapDecibelSlider : public Component, public Slider::Listener
        {
            WrapDecibelSlider()
            {
                decibelSlider.setRange(-60, 0);
                decibelSlider.setSkewFactorFromMidPoint(-10);
                decibelSlider.setTextBoxStyle(Slider::TextBoxRight, false, 100, 20);
                decibelSlider.setValue(Decibels::gainToDecibels(currentLevel));
                decibelSlider.addListener(this);
                addAndMakeVisible(&decibelSlider);

                decibelLabel.setText("Gain", dontSendNotification);
                addAndMakeVisible(&decibelLabel);
            }

            void resized() override
            {
                juce::FlexBox fb;

                juce::FlexItem left(getWidth() * 0.2f, getHeight(), decibelLabel);
                juce::FlexItem right(getWidth() * 0.8f, getHeight(), decibelSlider);

                fb.items.addArray({ left, right });
                fb.performLayout(getLocalBounds().toFloat());
            }

            void sliderValueChanged(Slider* slider) override
            {
                targetLevel = Decibels::decibelsToGain((float) decibelSlider.getValue());
            }

            DecibelSlider decibelSlider;
            Label decibelLabel;
            float currentLevel = 0.3f, targetLevel = 0.3f;
        };

        WrapDecibelSlider wrapDecibelSlider;
    };

    struct BodyPanel : public Component
    {
        BodyPanel(Music::Metre& metre) : metreListPanel(metre)
        {
            addAndMakeVisible(&metreListPanel);
        }

        void resized() override
        {
            auto bounds = getLocalBounds();
            metreListPanel.setBounds(bounds);
            settingPanel.setBounds(bounds);
        }

        void setCurrentPanel(Component* panelToUse)
        {
            if (panelToUse != currentPanel)
            {
                if (currentPanel != nullptr)
                {
                    currentPanel->setVisible(false);
                    removeChildComponent(currentPanel);
                }

                currentPanel = panelToUse;

                if (panelToUse != nullptr)
                {
                    addChildComponent(panelToUse);
                    panelToUse->setVisible(true);
                    panelToUse->toFront(true);
                }
            }
        }

        MetreListPanel metreListPanel;
        SettingPanel settingPanel;
        Component* currentPanel = &metreListPanel;
    };

    AppLookAndFeel appLookAndFeel;

    Music::Metre musicMetre;
    BeatAudioSource beatAudioSource;

    TransportState state;

    HeaderPanel headerPanel;
    BodyPanel bodyPanel;

    float& currentLevel = bodyPanel.settingPanel.wrapDecibelSlider.currentLevel;
    float& targetLevel = bodyPanel.settingPanel.wrapDecibelSlider.targetLevel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

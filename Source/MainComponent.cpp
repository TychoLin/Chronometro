/*
  ==============================================================================

    This file was auto-generated!

  ==============================================================================
*/

#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
    : beatAudioSource(musicMetre),
      headerPanel(musicMetre),
      bodyPanel(musicMetre)
{
    // Make sure you set the size of the component after
    // you add any child components.
    setLookAndFeel(&appLookAndFeel);

    addAndMakeVisible(&headerPanel);
    addAndMakeVisible(&bodyPanel);

    bodyPanel.settingPanel.soundStallProcessorEditor.reset(beatAudioSource.createEditor());
    bodyPanel.settingPanel.addAndMakeVisible(*bodyPanel.settingPanel.soundStallProcessorEditor);

    beatAudioSource.addChangeListener(this);

    // setSize(640, 360);
    setSize(360, 640);

    // Some platforms require permissions to open input channels so request that here
    if (RuntimePermissions::isRequired(RuntimePermissions::recordAudio)
        && !RuntimePermissions::isGranted(RuntimePermissions::recordAudio))
    {
        RuntimePermissions::request(RuntimePermissions::recordAudio,
                                    [&](bool granted) { if (granted)  setAudioChannels (0, 2); });
    }
    else
    {
        // Specify the number of input and output channels that we want to open
        setAudioChannels(0, 2);
    }
}

MainComponent::~MainComponent()
{
    // This shuts down the audio device and clears the audio source.
    shutdownAudio();
}

//==============================================================================
void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    // This function will be called when the audio device is started, or when
    // its settings (i.e. sample rate, block size, etc) are changed.

    // You can use this function to initialise any resources you might need,
    // but be careful - it will be called on the audio thread, not the GUI thread.

    // For more details, see the help for AudioProcessor::prepareToPlay()
    beatAudioSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
    bodyPanel.metreListPanel.init(); // initialize after beatAudioSource get sampleRate
}

void MainComponent::getNextAudioBlock(const AudioSourceChannelInfo& bufferToFill)
{
    // Your audio-processing code goes here!

    // For more details, see the help for AudioProcessor::getNextAudioBlock()

    // Right now we are not producing any data, in which case we need to clear the buffer
    // (to prevent the output of random noise)
    // bufferToFill.clearActiveBufferRegion();
    beatAudioSource.getNextAudioBlock(bufferToFill);

    auto localTargetLevel = targetLevel;
    bufferToFill.buffer->applyGainRamp(bufferToFill.startSample, bufferToFill.numSamples, currentLevel, localTargetLevel);
    currentLevel = localTargetLevel;
}

void MainComponent::releaseResources()
{
    // This will be called when the audio device stops, or when it is being
    // restarted due to a setting change.

    // For more details, see the help for AudioProcessor::releaseResources()
    beatAudioSource.releaseResources();
}

//==============================================================================
void MainComponent::paint(Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));

    // You can add your drawing code here!
}

void MainComponent::resized()
{
    // This is called when the MainContentComponent is resized.
    // If you add any child components, this is where you should
    // update their positions.
    auto isPortrait = getHeight() > getWidth();

    FlexBox fbGroup;
    FlexBox fbContainer;
    float margin = 8.0f;

    if (isPortrait)
    {
        auto width = getWidth() - 2.0f * margin;
        auto height = getHeight() - 3.0f * margin;

        fbGroup.flexDirection = FlexBox::Direction::column;

        FlexItem header(width, 123.2f, headerPanel);
        FlexItem body(width, height * 0.8f, bodyPanel);

        FlexItem::Margin headerPanelMargin;
        headerPanelMargin.bottom = margin;
        fbGroup.items.addArray({ header.withMargin(headerPanelMargin), body });
    }
    else
    {
        auto width = getWidth() - 3.0f * margin;
        auto height = getHeight() - 2.0f * margin;

        fbGroup.flexDirection = FlexBox::Direction::row;

        FlexItem header(width * 0.25f, height, headerPanel);
        FlexItem body(width * 0.75f, height, bodyPanel);

        FlexItem::Margin headerPanelMargin;
        headerPanelMargin.right = margin;
        fbGroup.items.addArray({ header.withMargin(headerPanelMargin), body });
    }

    fbContainer.items.add(FlexItem(fbGroup).withMargin({ margin }));
    fbContainer.performLayout(getLocalBounds().toFloat());
}

void MainComponent::changeListenerCallback(ChangeBroadcaster* source)
{
    if (source == &beatAudioSource)
    {
        if (beatAudioSource.isPlaying())
            changeState(Playing);
        else
            changeState(Stopped);
    }
}

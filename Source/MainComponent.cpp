/*
  ==============================================================================

    This file was auto-generated!

  ==============================================================================
*/

#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent() : beatAudioSource(musicMetre)
{
    // Make sure you set the size of the component after
    // you add any child components.
    setLookAndFeel(&appLookAndFeel);

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
    startButton.setColour(TextButton::buttonOnColourId, Colours::blanchedalmond);
    startButton.onClick = [this] { playButtonClicked(); };
    addAndMakeVisible(&startButton);

    stopButton.setButtonText("Stop");
    stopButton.onClick = [this] {
        startButton.setToggleState(false, dontSendNotification);
        stopButtonClicked();
    };
    addAndMakeVisible(&stopButton);

    beatAudioSource.addChangeListener(this);

    // addAndMakeVisible(&visualBeatRegion);

    setSize(400, 300);

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
}

void MainComponent::getNextAudioBlock(const AudioSourceChannelInfo& bufferToFill)
{
    // Your audio-processing code goes here!

    // For more details, see the help for AudioProcessor::getNextAudioBlock()

    // Right now we are not producing any data, in which case we need to clear the buffer
    // (to prevent the output of random noise)
    // bufferToFill.clearActiveBufferRegion();
    beatAudioSource.getNextAudioBlock(bufferToFill);
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
    g.fillAll(appLookAndFeel.findColour(ResizableWindow::backgroundColourId));

    // You can add your drawing code here!
}

void MainComponent::resized()
{
    // This is called when the MainContentComponent is resized.
    // If you add any child components, this is where you should
    // update their positions.
    auto headerHeight = 40;
    auto headerPadding = 10;
    auto area = getLocalBounds();
    auto headArea = area.removeFromTop(headerHeight).reduced(headerPadding);

    auto itemMargin = 3;
    tapButton.setBounds(headArea.withWidth(40));
    headArea.removeFromLeft(tapButton.getWidth() + itemMargin);
    BPMLabel.setBounds(headArea.withWidth(60));
    headArea.removeFromLeft(BPMLabel.getWidth() + itemMargin);
    startButton.setBounds(headArea.withWidth(40));
    headArea.removeFromLeft(startButton.getWidth() + itemMargin);
    stopButton.setBounds(headArea.withWidth(40));

    // visualBeatRegion.setBounds(10, 10 + 20 + 10, 100, 60);
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

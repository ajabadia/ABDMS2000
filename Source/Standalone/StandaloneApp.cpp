#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../Plugin/PluginProcessor.h"
#include "../Core/AppLogger.h"
#include <iostream>

namespace ABDMS2000 {

class ABDMS2000StandaloneApp : public juce::JUCEApplication
{
public:
    ABDMS2000StandaloneApp() {}

    const juce::String getApplicationName() override { return "ABDMS2000"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String& /*commandLine*/) override
    {
        ABD_LOG("=== [STANDALONE] ABDMS2000StandaloneApp::initialise() start ===");

        // 1. Initialize Settings
        juce::PropertiesFile::Options options;
        options.applicationName = "ABDMS2000";
        options.filenameSuffix = ".settings";
        options.folderName = "ajabadia";
        options.osxLibrarySubFolder = "Application Support";
        settings = std::make_unique<juce::PropertiesFile>(options);

        // 2. Initialize Audio Device Manager
        ABD_LOG("[STANDALONE] Initializing AudioDeviceManager...");
        juce::String audioError;
        if (settings != nullptr)
        {
            auto xml = settings->getXmlValue("audioDeviceState");
            if (xml != nullptr)
            {
                audioError = deviceManager.initialise(0, 2, xml.get(), true);
                if (audioError.isNotEmpty())
                    ABD_LOG("[STANDALONE] Saved audio config error: " + audioError);
            }
        }

        if (deviceManager.getCurrentAudioDevice() == nullptr)
        {
            audioError = deviceManager.initialiseWithDefaultDevices(0, 2);
            if (deviceManager.getCurrentAudioDevice() == nullptr)
            {
                ABD_LOG("[STANDALONE] Fallback to initialise(2, 2)...");
                audioError = deviceManager.initialise(2, 2, nullptr, true);
            }
        }

        if (deviceManager.getCurrentAudioDevice() != nullptr)
        {
            ABD_LOG(juce::String("[STANDALONE] Audio Device Ready: ") + deviceManager.getCurrentAudioDevice()->getName()
                    + " (" + juce::String(deviceManager.getCurrentAudioDevice()->getCurrentSampleRate()) + " Hz, "
                    + juce::String(deviceManager.getCurrentAudioDevice()->getCurrentBufferSizeSamples()) + " smp)");
        }
        else
        {
            ABD_LOG("[STANDALONE] ERROR: No audio device could be initialized. Error: " + audioError);
        }

        // 3. Create AudioProcessor
        ABD_LOG("[STANDALONE] Creating ABDMS2000AudioProcessor...");
        auto processor = std::make_unique<ABDMS2000AudioProcessor>();

        // 4. Create Main Window
        ABD_LOG("[STANDALONE] Creating MainWindow...");
        mainWindow.reset(new MainWindow(getApplicationName(), std::move(processor), settings.get(), deviceManager));

        // 5. Auto-connect MIDI inputs
        auto midiInputs = juce::MidiInput::getAvailableDevices();
        for (auto& device : midiInputs)
        {
            if (!deviceManager.isMidiInputDeviceEnabled(device.identifier))
            {
                deviceManager.setMidiInputDeviceEnabled(device.identifier, true);
                ABD_LOG(juce::String("[STANDALONE] Auto-connected MIDI Input: ") + device.name);
            }
        }

        mainWindow->syncMidiCallbacks();
        mainWindow->setVisible(true);
        ABD_LOG("=== [STANDALONE] MainWindow visible, running event loop ===");
    }

    void shutdown() override
    {
        ABD_LOG("=== [STANDALONE] ABDMS2000StandaloneApp::shutdown() ===");
        if (mainWindow != nullptr)
            mainWindow->setVisible(false);

        mainWindow = nullptr;
        settings = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String&) override {}

    //==============================================================================
    class MainWindow : public juce::DocumentWindow,
                       private juce::ChangeListener,
                       public juce::MenuBarModel
    {
    public:
        ABDMS2000AudioProcessor* getProcessor() const { return dynamic_cast<ABDMS2000AudioProcessor*>(processor_.get()); }

        MainWindow(const juce::String& name, std::unique_ptr<juce::AudioProcessor> createdProcessor,
                   juce::PropertiesFile* settings, juce::AudioDeviceManager& dm)
            : DocumentWindow(name, juce::Colour(0xff12141a), juce::DocumentWindow::allButtons),
              processor_(std::move(createdProcessor)),
              deviceManager_(dm),
              settings_(settings)
        {
            setUsingNativeTitleBar(true);
            setResizable(true, true);
            setResizeLimits(800, 500, 1920, 1200);
            setTitleBarButtonsRequired(juce::DocumentWindow::allButtons, false);

            setMenuBar(this);

            // Connect Audio Processor to player
            player_.setProcessor(processor_.get());
            deviceManager_.addAudioCallback(&player_);

            createEditor();
            deviceManager_.addChangeListener(this);
        }

        ~MainWindow() override
        {
            settingsWindow = nullptr;
            deviceManager_.removeChangeListener(this);
            deviceManager_.removeAudioCallback(&player_);

            auto midiInputs = juce::MidiInput::getAvailableDevices();
            for (auto& device : midiInputs)
            {
                if (deviceManager_.isMidiInputDeviceEnabled(device.identifier))
                    deviceManager_.removeMidiInputDeviceCallback(device.identifier, &player_);
            }

            if (settings_ != nullptr)
            {
                if (auto xml = deviceManager_.createStateXml())
                {
                    settings_->setValue("audioDeviceState", xml.get());
                    settings_->saveIfNeeded();
                }
            }

            player_.setProcessor(nullptr);
            setContentOwned(nullptr, true);
            setMenuBar(nullptr);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

        void changeListenerCallback(juce::ChangeBroadcaster*) override
        {
            if (deviceManager_.getCurrentAudioDevice() != nullptr)
            {
                ABD_LOG(juce::String("[STANDALONE] Audio device changed: ") + deviceManager_.getCurrentAudioDevice()->getName());
            }
            if (settings_ != nullptr)
            {
                if (auto xml = deviceManager_.createStateXml())
                {
                    settings_->setValue("audioDeviceState", xml.get());
                    settings_->saveIfNeeded();
                }
            }
        }

        void syncMidiCallbacks()
        {
            auto midiInputs = juce::MidiInput::getAvailableDevices();
            for (auto& device : midiInputs)
            {
                if (deviceManager_.isMidiInputDeviceEnabled(device.identifier))
                {
                    deviceManager_.removeMidiInputDeviceCallback(device.identifier, &player_);
                    deviceManager_.addMidiInputDeviceCallback(device.identifier, &player_);
                }
            }
        }

        juce::StringArray getMenuBarNames() override
        {
            return { "Options" };
        }

        juce::PopupMenu getMenuForIndex(int topLevelMenuIndex, const juce::String&) override
        {
            juce::PopupMenu menu;
            if (topLevelMenuIndex == 0)
            {
                menu.addItem(1, "Audio / MIDI Settings...");
            }
            return menu;
        }

        void menuItemSelected(int menuItemID, int) override
        {
            if (menuItemID == 1)
                showAudioSettings();
        }

    private:
        void createEditor()
        {
            if (auto* editor = processor_->createEditor())
            {
                setContentOwned(editor, true);
            }
            else
            {
                auto* l = new juce::Label();
                l->setText("Error: Editor could not be created.", juce::dontSendNotification);
                l->setSize(600, 400);
                setContentOwned(l, true);
            }
        }

        void showAudioSettings()
        {
            if (settingsWindow != nullptr)
            {
                settingsWindow->toFront(true);
                return;
            }

            juce::DialogWindow::LaunchOptions opt;
            opt.dialogTitle = "ABDMS2000 - Audio / MIDI Settings";
            opt.dialogBackgroundColour = juce::Colour(0xff181a20);
            opt.escapeKeyTriggersCloseButton = true;
            opt.useNativeTitleBar = true;
            opt.resizable = false;

            auto* selector = new juce::AudioDeviceSelectorComponent(deviceManager_,
                0, 256, 0, 256,
                true, true,
                true, false);

            selector->setSize(500, 450);
            opt.content.setOwned(selector);

            settingsWindow = opt.launchAsync();
        }

        juce::AudioDeviceManager& deviceManager_;
        juce::AudioProcessorPlayer player_;
        std::unique_ptr<juce::AudioProcessor> processor_;
        juce::PropertiesFile* settings_{ nullptr };
        juce::Component::SafePointer<juce::DialogWindow> settingsWindow;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

    juce::AudioDeviceManager deviceManager;

private:
    std::unique_ptr<MainWindow> mainWindow;
    std::unique_ptr<juce::PropertiesFile> settings;
};

} // namespace ABDMS2000

//==============================================================================
START_JUCE_APPLICATION(ABDMS2000::ABDMS2000StandaloneApp)

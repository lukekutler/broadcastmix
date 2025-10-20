#include "MainComponent.h"

#include <core/Application.h>

#include <juce_gui_extra/juce_gui_extra.h>

namespace broadcastmix::app {

class MainWindow : public juce::DocumentWindow {
public:
    MainWindow(juce::String title, core::Application& app)
        : DocumentWindow(title,
                         juce::Colour::fromFloatRGBA(0.07F, 0.07F, 0.09F, 1.0F),
                         DocumentWindow::allButtons) {
        setUsingNativeTitleBar(true);
        setResizable(true, false);
        setContentOwned(new MainComponent(app), true);
        centreWithSize(900, 600);
        setVisible(true);
    }

    void closeButtonPressed() override {
        juce::JUCEApplicationBase::quit();
    }
};

class BroadcastMixApplication final : public juce::JUCEApplication {
public:
    BroadcastMixApplication() = default;

    const juce::String getApplicationName() override {
        return JUCE_APPLICATION_NAME_STRING;
    }

    const juce::String getApplicationVersion() override {
        return JUCE_APPLICATION_VERSION_STRING;
    }

    void initialise(const juce::String&) override {
        coreApp_ = std::make_unique<broadcastmix::core::Application>(
            broadcastmix::core::ApplicationConfig {},
            broadcastmix::audio::AudioEngineSettings {});

        coreApp_->initialize();
        coreApp_->startRealtimeEngine();

        mainWindow_ = std::make_unique<MainWindow>(getApplicationName(), *coreApp_);
    }

    void shutdown() override {
        if (coreApp_) {
            coreApp_->stopRealtimeEngine();
            coreApp_.reset();
        }
        mainWindow_.reset();
    }

private:
    std::unique_ptr<MainWindow> mainWindow_;
    std::unique_ptr<broadcastmix::core::Application> coreApp_;
};

} // namespace broadcastmix::app

START_JUCE_APPLICATION(broadcastmix::app::BroadcastMixApplication)

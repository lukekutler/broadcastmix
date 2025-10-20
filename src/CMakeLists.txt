add_library(broadcastmix STATIC)

target_include_directories(broadcastmix
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(broadcastmix
    PRIVATE
        audio/AudioEngine.cpp
        audio/GraphNode.cpp
        audio/GraphTopology.cpp
        audio/JuceGraphBuilder.cpp
        audio/MeterStore.cpp
        audio/processors/GainProcessor.cpp
        audio/processors/PassThroughProcessor.cpp
        audio/processors/SignalGeneratorProcessor.cpp
        core/Application.cpp
        core/Logging.cpp
        persistence/ProjectSerializer.cpp
        plugins/PluginHost.cpp
        ui/NodeGraphView.cpp
        ui/UiTheme.cpp
        control/ControlSurfaceManager.cpp
        update/UpdateService.cpp
)

target_compile_definitions(broadcastmix
    PUBLIC
        $<$<CONFIG:Debug>:BROADCASTMIX_DEBUG_BUILD>
)

target_compile_features(broadcastmix PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(broadcastmix PRIVATE /W4 /permissive-)
else()
    target_compile_options(broadcastmix PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BROADCASTMIX_FETCH_JUCE)
    target_link_libraries(broadcastmix
        PUBLIC
            juce::juce_audio_devices
            juce::juce_audio_processors
            juce::juce_audio_utils
            juce::juce_audio_basics
            juce::juce_events
            juce::juce_graphics
            juce::juce_core
    )
    target_compile_definitions(broadcastmix PUBLIC BROADCASTMIX_HAS_JUCE=1)
else()
    target_compile_definitions(broadcastmix PUBLIC BROADCASTMIX_HAS_JUCE=0)
endif()

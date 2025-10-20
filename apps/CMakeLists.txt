if(NOT BROADCASTMIX_FETCH_JUCE)
    return()
endif()

juce_add_gui_app(BroadcastMixApp
    PRODUCT_NAME "BroadcastMix"
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "BroadcastMix"
    BUNDLE_IDENTIFIER "com.broadcastmix.app"
)

target_sources(BroadcastMixApp
    PRIVATE
        BroadcastMixApp/Main.cpp
        BroadcastMixApp/MainComponent.cpp
        BroadcastMixApp/MainComponent.h
        BroadcastMixApp/NodeGraphComponent.cpp
        BroadcastMixApp/NodeGraphComponent.h
        BroadcastMixApp/NodeLibraryComponent.cpp
        BroadcastMixApp/NodeLibraryComponent.h
)

target_compile_definitions(BroadcastMixApp
    PRIVATE
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:BroadcastMixApp,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
        JUCE_STRICT_REFCOUNTEDPOINTER=1
)

target_link_libraries(BroadcastMixApp
    PRIVATE
        broadcastmix
        juce::juce_gui_extra
        juce::juce_gui_basics
        juce::juce_audio_utils
        juce::juce_audio_basics
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_core
)

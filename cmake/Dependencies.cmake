include(FetchContent)

set(JUCE_GIT_REPOSITORY "https://github.com/juce-framework/JUCE.git" CACHE STRING "JUCE repository URL")
set(JUCE_GIT_TAG "develop" CACHE STRING "JUCE version tag or branch")

if(NOT TARGET juce::juce_recommended_config_flags)
    message(STATUS "Fetching JUCE from ${JUCE_GIT_REPOSITORY} (${JUCE_GIT_TAG})")
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY ${JUCE_GIT_REPOSITORY}
        GIT_TAG ${JUCE_GIT_TAG}
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

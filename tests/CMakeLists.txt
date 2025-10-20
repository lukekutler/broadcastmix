add_executable(broadcastmix_tests
    test_main.cpp
)

target_link_libraries(broadcastmix_tests PRIVATE broadcastmix)

add_test(NAME smoke COMMAND broadcastmix_tests)

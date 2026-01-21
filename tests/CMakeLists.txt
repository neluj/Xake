add_executable(chess_tests
    test_fen.cpp
)

target_link_libraries(chess_tests PRIVATE chess_core Qt6::Test)

add_test(NAME chess_tests COMMAND chess_tests)

add_executable(match_settings_tests
    test_match_validation.cpp
    ${CMAKE_SOURCE_DIR}/app/match_settings_validation.cpp
)

target_include_directories(match_settings_tests PRIVATE ${CMAKE_SOURCE_DIR}/app)
target_link_libraries(match_settings_tests PRIVATE Qt6::Test chess_core)

add_test(NAME match_settings_tests COMMAND match_settings_tests)

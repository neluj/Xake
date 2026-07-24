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

add_executable(match_settings_widget_tests
    test_match_settings_widget.cpp
    ${CMAKE_SOURCE_DIR}/app/match_settings_widget.cpp
    ${CMAKE_SOURCE_DIR}/app/match_settings_widget.ui
)

target_include_directories(match_settings_widget_tests PRIVATE ${CMAKE_SOURCE_DIR}/app)
target_link_libraries(match_settings_widget_tests PRIVATE Qt6::Test Qt6::Widgets)

add_test(NAME match_settings_widget_tests COMMAND match_settings_widget_tests)

add_executable(move_execution_tests
    test_move_execution.cpp
    ${CMAKE_SOURCE_DIR}/app/game_controller.cpp
    ${CMAKE_SOURCE_DIR}/app/match_settings_validation.cpp
    ${CMAKE_SOURCE_DIR}/app/uci_client.cpp
)

target_include_directories(move_execution_tests PRIVATE ${CMAKE_SOURCE_DIR}/app)
target_link_libraries(move_execution_tests PRIVATE Qt6::Test chess_core)

add_test(NAME move_execution_tests COMMAND move_execution_tests)

add_executable(perft_tests
    test_perft.cpp
)

target_link_libraries(perft_tests PRIVATE Qt6::Test chess_core)

add_test(NAME perft_tests COMMAND perft_tests)

add_executable(perft_runner
    perft_runner.cpp
)

target_link_libraries(perft_runner PRIVATE chess_core)

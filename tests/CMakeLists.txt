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

add_executable(board_widget_tests
    test_boardwidget.cpp
    ${CMAKE_SOURCE_DIR}/app/boardwidget.cpp
    ${CMAKE_SOURCE_DIR}/app/captured_pieces_widget.cpp
    ${CMAKE_SOURCE_DIR}/app/resources.qrc
)

target_include_directories(board_widget_tests PRIVATE ${CMAKE_SOURCE_DIR}/app)
target_link_libraries(board_widget_tests PRIVATE
    Qt6::Test
    Qt6::Widgets
    Qt6::Svg
    chess_core
)

add_test(NAME board_widget_tests COMMAND board_widget_tests)

add_executable(storage_paths_tests
    test_storage_paths.cpp
    ${CMAKE_SOURCE_DIR}/app/storage_paths.cpp
)

target_include_directories(storage_paths_tests PRIVATE ${CMAKE_SOURCE_DIR}/app)
target_link_libraries(storage_paths_tests PRIVATE Qt6::Test)

add_test(NAME storage_paths_tests COMMAND storage_paths_tests)

add_executable(app_settings_tests
    test_app_settings.cpp
    ${CMAKE_SOURCE_DIR}/app/app_settings.cpp
)

target_include_directories(app_settings_tests PRIVATE ${CMAKE_SOURCE_DIR}/app)
target_link_libraries(app_settings_tests PRIVATE Qt6::Test)

add_test(NAME app_settings_tests COMMAND app_settings_tests)

add_executable(application_data_tests
    test_application_data.cpp
    ${CMAKE_SOURCE_DIR}/app/application_data.cpp
    ${CMAKE_SOURCE_DIR}/app/application_data_dialog.cpp
)

target_include_directories(application_data_tests PRIVATE ${CMAKE_SOURCE_DIR}/app)
target_link_libraries(application_data_tests PRIVATE Qt6::Test Qt6::Widgets)

add_test(NAME application_data_tests COMMAND application_data_tests)

add_executable(history_repository_tests
    test_history_repository.cpp
    ${CMAKE_SOURCE_DIR}/app/history_repository.cpp
)

target_include_directories(history_repository_tests PRIVATE ${CMAKE_SOURCE_DIR}/app)
target_link_libraries(history_repository_tests PRIVATE Qt6::Test)

add_test(NAME history_repository_tests COMMAND history_repository_tests)

add_executable(session_record_tests
    test_session_record.cpp
    ${CMAKE_SOURCE_DIR}/app/move_record.cpp
    ${CMAKE_SOURCE_DIR}/app/session_record.cpp
)

target_include_directories(session_record_tests PRIVATE ${CMAKE_SOURCE_DIR}/app)
target_link_libraries(session_record_tests PRIVATE Qt6::Test chess_core)

add_test(NAME session_record_tests COMMAND session_record_tests)

add_executable(opening_book_tests
    test_opening_book.cpp
    ${CMAKE_SOURCE_DIR}/app/opening_book.cpp
)

target_include_directories(opening_book_tests PRIVATE ${CMAKE_SOURCE_DIR}/app)
target_link_libraries(opening_book_tests PRIVATE Qt6::Test chess_core)

add_test(NAME opening_book_tests COMMAND opening_book_tests)

add_executable(pgn_export_tests
    test_pgn_export.cpp
    ${CMAKE_SOURCE_DIR}/app/pgn_export.cpp
)

target_include_directories(pgn_export_tests PRIVATE ${CMAKE_SOURCE_DIR}/app)
target_link_libraries(pgn_export_tests PRIVATE Qt6::Test chess_core)

add_test(NAME pgn_export_tests COMMAND pgn_export_tests)

add_executable(game_replay_tests
    test_game_replay.cpp
    ${CMAKE_SOURCE_DIR}/app/game_replay.cpp
    ${CMAKE_SOURCE_DIR}/app/move_record.cpp
    ${CMAKE_SOURCE_DIR}/app/opening_book.cpp
)

target_include_directories(game_replay_tests PRIVATE ${CMAKE_SOURCE_DIR}/app)
target_link_libraries(game_replay_tests PRIVATE Qt6::Test chess_core)

add_test(NAME game_replay_tests COMMAND game_replay_tests)

add_executable(move_execution_tests
    test_move_execution.cpp
    ${CMAKE_SOURCE_DIR}/app/game_controller.cpp
    ${CMAKE_SOURCE_DIR}/app/move_record.cpp
    ${CMAKE_SOURCE_DIR}/app/match_settings_validation.cpp
    ${CMAKE_SOURCE_DIR}/app/storage_paths.cpp
    ${CMAKE_SOURCE_DIR}/app/uci_client.cpp
)

target_include_directories(move_execution_tests PRIVATE ${CMAKE_SOURCE_DIR}/app)
target_link_libraries(move_execution_tests PRIVATE Qt6::Test chess_core)

add_test(NAME move_execution_tests COMMAND move_execution_tests)

add_executable(tournament_runner_tests
    test_tournament_runner.cpp
    ${CMAKE_SOURCE_DIR}/app/game_controller.cpp
    ${CMAKE_SOURCE_DIR}/app/match_settings_validation.cpp
    ${CMAKE_SOURCE_DIR}/app/move_record.cpp
    ${CMAKE_SOURCE_DIR}/app/pgn_export.cpp
    ${CMAKE_SOURCE_DIR}/app/storage_paths.cpp
    ${CMAKE_SOURCE_DIR}/app/tournament_runner.cpp
    ${CMAKE_SOURCE_DIR}/app/uci_client.cpp
)

target_include_directories(tournament_runner_tests PRIVATE ${CMAKE_SOURCE_DIR}/app)
target_link_libraries(tournament_runner_tests PRIVATE Qt6::Test chess_core)

add_test(NAME tournament_runner_tests COMMAND tournament_runner_tests)

add_executable(perft_tests
    test_perft.cpp
)

target_link_libraries(perft_tests PRIVATE Qt6::Test chess_core)

add_test(NAME perft_tests COMMAND perft_tests)

add_executable(perft_epd_tests
    test_perft_suite.cpp
)

target_compile_definitions(perft_epd_tests PRIVATE
    PERFT_SUITE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/perftsuite.epd"
)
target_link_libraries(perft_epd_tests PRIVATE Qt6::Test chess_core)

add_test(NAME perft_epd_tests COMMAND perft_epd_tests)
set_tests_properties(perft_epd_tests PROPERTIES
    LABELS "perft;release"
    TIMEOUT 300
)

add_executable(perft_runner
    perft_runner.cpp
)

target_link_libraries(perft_runner PRIVATE chess_core)

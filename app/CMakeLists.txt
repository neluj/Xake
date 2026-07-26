set(APP_SOURCES
    boardwidget.cpp
    boardwidget.h
    app_state.h
    game_controller.cpp
    game_controller.h
    history_repository.cpp
    history_repository.h
    match_settings_widget.cpp
    match_settings_widget.h
    match_settings_widget.ui
    match_settings_types.h
    match_settings_validation.cpp
    match_settings_validation.h
    opening_book.cpp
    opening_book.h
    pgn_export.cpp
    pgn_export.h
    uci_client.cpp
    uci_client.h
    single_game_dialog.cpp
    single_game_dialog.h
    single_game_dialog.ui
    tournament_dialog.cpp
    tournament_dialog.h
    tournament_dialog.ui
    tournament_runner.cpp
    tournament_runner.h
    session_record.cpp
    session_record.h
    storage_paths.cpp
    storage_paths.h
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    resources.qrc
)

if(WIN32)
    list(APPEND APP_SOURCES xake.rc)
endif()

qt_add_executable(Xake
    MANUAL_FINALIZATION
    ${APP_SOURCES}
)

target_include_directories(Xake PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Xake PRIVATE chess_core Qt6::Widgets Qt6::Svg)

set_target_properties(Xake PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

qt_finalize_executable(Xake)

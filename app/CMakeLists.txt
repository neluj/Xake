set(APP_SOURCES
    boardwidget.cpp
    boardwidget.h
    match_settings_widget.cpp
    match_settings_widget.h
    match_settings_widget.ui
    match_settings_types.h
    match_settings_validation.cpp
    match_settings_validation.h
    uci_client.cpp
    uci_client.h
    single_game_dialog.cpp
    single_game_dialog.h
    single_game_dialog.ui
    tournament_dialog.cpp
    tournament_dialog.h
    tournament_dialog.ui
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    resources.qrc
)

qt_add_executable(chess_app
    MANUAL_FINALIZATION
    ${APP_SOURCES}
)

target_include_directories(chess_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(chess_app PRIVATE chess_core Qt6::Widgets)

set_target_properties(chess_app PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

qt_finalize_executable(chess_app)

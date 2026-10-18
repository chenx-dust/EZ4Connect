cmake_minimum_required(VERSION 3.12)
project(EZ4Connect)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(NIGHTLY_BUILD    "Set if built nightly.")

if(NIGHTLY_BUILD)
    set(PROJ_VER "${GIT_HASH}-nightly")
elseif(DEFINED REL_VER)
    set(PROJ_VER "${REL_VER}")
else()
    set(PROJ_VER "dev")
endif()

add_compile_definitions(PROJ_VER="${PROJ_VER}")
message("PROJ_VER=${PROJ_VER}")

find_package(Qt6 6.5 COMPONENTS
    Core
    Concurrent
    Gui
    Widgets
    Network
    Svg
    Core5Compat
    WebEngineWidgets
    REQUIRED
)


set(SOURCE_FILES
    main.cpp
    presentation/main/mainwindow.cpp
    presentation/main/mainwindowcoordinator.cpp
    presentation/coordinators/authdialogcoordinator.cpp
    presentation/coordinators/connectionuicontroller.cpp
    application/applicationlogger.cpp
    application/commandlineoptions.cpp
    application/coreprocess.h
    application/connectionsession.cpp
    application/profileservice.cpp
    application/settingsmigrator.cpp
    application/systemproxysession.cpp
    application/defaultsettings.cpp
    infrastructure/coreprocess/corecommandbuilder.cpp
    infrastructure/coreprocess/consoleoutputdecoder.cpp
    infrastructure/coreprocess/coreexecutable.cpp
    infrastructure/coreprocess/devicetrust.cpp
    infrastructure/coreprocess/coreoutputbuffer.cpp
    infrastructure/coreprocess/coreoutputparser.cpp
    core/connectionsessionstate.cpp
    infrastructure/logging/applicationlogfile.cpp
    infrastructure/platform/autostart.cpp
    infrastructure/platform/privileges.cpp
    infrastructure/settings/settingsprofileloader.cpp
    infrastructure/storage/applicationpaths.cpp
    infrastructure/update/updatechecker.cpp
    infrastructure/platform/platformsystemproxybackend.cpp
    infrastructure/platform/terminationwatcher.cpp
    infrastructure/platform/systemproxyfunctions.cpp
    infrastructure/coreprocess/zjuconnectprocess.cpp
    presentation/presentationhelpers.cpp
    presentation/dialogs/extrasettingwindow/extrasettingwindow.cpp
    presentation/dialogs/settingwindow/settingwindow.cpp
    presentation/dialogs/loginwindow/loginwindow.cpp
    presentation/dialogs/sudowindow/sudowindow.cpp
    presentation/dialogs/ssologinwebview/ssologinwebview.cpp
    presentation/dialogs/authinfowindow/authinfowindow.cpp
    presentation/dialogs/configurationguidedialog/configurationguidedialog.cpp
    presentation/dialogs/graphcaptchawindow/graphcaptchawindow.cpp
    presentation/dialogs/graphcaptchawindow/captchacanvas.cpp
    infrastructure/settings/profilemanager.cpp
    resource.qrc
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

if (WIN32)
    message("Building for Windows")

    enable_language("RC")

    add_executable(${PROJECT_NAME} WIN32
        ${SOURCE_FILES}
        ${CMAKE_CURRENT_SOURCE_DIR}/resource/icon.rc
    )

    set(PLATFORM_LIBS wininet rasapi32 shell32)
elseif(APPLE)
    message("Building for MacOS")
    
    set(MACOSX_BUNDLE_ICON_FILE icon.icns)
    
    set(app_icon_macos ${CMAKE_CURRENT_SOURCE_DIR}/resource/icon.icns)
    set_source_files_properties(${app_icon_macos} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

    add_executable(${PROJECT_NAME} MACOSX_BUNDLE
        ${SOURCE_FILES}
        ${app_icon_macos}
    )

    set_target_properties(${PROJECT_NAME}
        PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/resource/Info.plist")
else()
    add_executable(${PROJECT_NAME}
        ${SOURCE_FILES}
    )

    install(
        TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
    )
endif()

set(QAPPLICATION_CLASS QApplication CACHE STRING "Inheritance class for SingleApplication")

add_subdirectory(singleapplication)

target_link_libraries(${PROJECT_NAME}
        Qt::Core
        Qt::Concurrent
        Qt::Gui
        Qt::Widgets
        Qt::Network
        Qt::Svg
        Qt::Core5Compat
        Qt::WebEngineWidgets
        SingleApplication::SingleApplication
        ${PLATFORM_LIBS}
)

include(CTest)

if(BUILD_TESTING)
    add_executable(applicationlogger_test
        tests/applicationlogger_test.cpp
        application/applicationlogger.cpp
    )
    target_include_directories(applicationlogger_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(applicationlogger_test Qt::Core)
    add_test(NAME applicationlogger_test COMMAND applicationlogger_test)

    add_executable(applicationlogfile_test
        tests/applicationlogfile_test.cpp
        infrastructure/logging/applicationlogfile.cpp
    )
    target_include_directories(applicationlogfile_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(applicationlogfile_test Qt::Core)
    add_test(NAME applicationlogfile_test COMMAND applicationlogfile_test)

    add_executable(corecommandbuilder_test
        tests/corecommandbuilder_test.cpp
        infrastructure/coreprocess/corecommandbuilder.cpp
    )
    target_include_directories(corecommandbuilder_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(corecommandbuilder_test Qt::Core)
    add_test(NAME corecommandbuilder_test COMMAND corecommandbuilder_test)

    add_executable(coreoutputparser_test
        tests/coreoutputparser_test.cpp
        infrastructure/coreprocess/coreoutputparser.cpp
    )
    target_include_directories(coreoutputparser_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(coreoutputparser_test Qt::Core)
    add_test(NAME coreoutputparser_test COMMAND coreoutputparser_test)

    add_executable(coreoutputbuffer_test
        tests/coreoutputbuffer_test.cpp
        infrastructure/coreprocess/coreoutputbuffer.cpp
    )
    target_include_directories(coreoutputbuffer_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(coreoutputbuffer_test Qt::Core)
    add_test(NAME coreoutputbuffer_test COMMAND coreoutputbuffer_test)

    add_executable(settingsprofileloader_test
        tests/settingsprofileloader_test.cpp
        infrastructure/settings/settingsprofileloader.cpp
    )
    target_include_directories(settingsprofileloader_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(settingsprofileloader_test Qt::Core)
    add_test(NAME settingsprofileloader_test COMMAND settingsprofileloader_test)

    add_executable(connectionsessionstate_test
        tests/connectionsessionstate_test.cpp
        core/connectionsessionstate.cpp
    )
    target_include_directories(connectionsessionstate_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(connectionsessionstate_test Qt::Core)
    add_test(NAME connectionsessionstate_test COMMAND connectionsessionstate_test)

    add_executable(connectionsession_test
        tests/connectionsession_test.cpp
        application/coreprocess.h
        application/connectionsession.cpp
        core/connectionsessionstate.cpp
    )
    target_include_directories(connectionsession_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(connectionsession_test Qt::Core)
    add_test(NAME connectionsession_test COMMAND connectionsession_test)

    add_executable(systemproxysession_test
        tests/systemproxysession_test.cpp
        application/systemproxysession.cpp
    )
    target_include_directories(systemproxysession_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(systemproxysession_test Qt::Core Qt::Concurrent)
    add_test(NAME systemproxysession_test COMMAND systemproxysession_test)

    add_executable(updatechecker_test
        tests/updatechecker_test.cpp
        infrastructure/update/updatechecker.cpp
        infrastructure/coreprocess/coreexecutable.cpp
    )
    target_include_directories(updatechecker_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(updatechecker_test Qt::Core Qt::Gui Qt::Widgets Qt::Network Qt::Core5Compat)
    add_test(NAME updatechecker_test COMMAND updatechecker_test)

    add_executable(terminationwatcher_test
        tests/terminationwatcher_test.cpp
        infrastructure/platform/terminationwatcher.cpp
    )
    target_include_directories(terminationwatcher_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(terminationwatcher_test Qt::Core)
    add_test(NAME terminationwatcher_test COMMAND terminationwatcher_test)

    add_executable(settingsmigrator_test
        tests/settingsmigrator_test.cpp
        application/settingsmigrator.cpp
        application/defaultsettings.cpp
    )
    target_include_directories(settingsmigrator_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(settingsmigrator_test Qt::Core)
    add_test(NAME settingsmigrator_test COMMAND settingsmigrator_test)

    add_executable(profileservice_test
        tests/profileservice_test.cpp
        application/profileservice.cpp
    )
    target_include_directories(profileservice_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(profileservice_test Qt::Core)
    add_test(NAME profileservice_test COMMAND profileservice_test)

    add_executable(profilemanager_test
        tests/profilemanager_test.cpp
        infrastructure/settings/profilemanager.cpp
    )
    target_include_directories(profilemanager_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(profilemanager_test Qt::Core)
    add_test(NAME profilemanager_test COMMAND profilemanager_test)

    add_executable(configurationguidedialog_test
        tests/configurationguidedialog_test.cpp
        presentation/dialogs/configurationguidedialog/configurationguidedialog.cpp
        presentation/dialogs/authinfowindow/authinfowindow.cpp
        infrastructure/coreprocess/consoleoutputdecoder.cpp
        infrastructure/coreprocess/coreexecutable.cpp
    )
    target_include_directories(configurationguidedialog_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(configurationguidedialog_test
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Core5Compat
    )
    add_test(NAME configurationguidedialog_test COMMAND configurationguidedialog_test)
    set_tests_properties(configurationguidedialog_test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    add_executable(graphcaptchawindow_test
        tests/graphcaptchawindow_test.cpp
        presentation/dialogs/graphcaptchawindow/graphcaptchawindow.cpp
        presentation/dialogs/graphcaptchawindow/captchacanvas.cpp
    )
    target_include_directories(graphcaptchawindow_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(graphcaptchawindow_test Qt::Core Qt::Gui Qt::Widgets)
    add_test(NAME graphcaptchawindow_test COMMAND graphcaptchawindow_test)
    set_tests_properties(graphcaptchawindow_test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()

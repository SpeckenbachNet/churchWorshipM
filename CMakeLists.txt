cmake_minimum_required(VERSION 3.16)

set(PROJECT_VERSION "0.75.0")
set(ORGANIZATION_NAME "SpeckenbachNet")
set(ORGANIZATION_DOMAIN "net.speckenbach")

project(
    ChurchWorshipM
    VERSION ${PROJECT_VERSION}
    LANGUAGES CXX
)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Pdf Network WebEngineWidgets Multimedia MultimediaWidgets Sql LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Pdf Network WebEngineWidgets Multimedia MultimediaWidgets Sql LinguistTools)
# QZipReader (SWORD import) is private Qt API
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS CorePrivate)

set(TS_FILES
    i18n/${PROJECT_NAME}_en.ts
    i18n/${PROJECT_NAME}_de.ts
)

set(PROJECT_SOURCES
        main.cpp
        mainwin.cpp
        mainwin.h
        mainwin.ui
)


if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(${PROJECT_NAME}
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        icons.qrc
        toolbarm.h
        toolbarm.cpp
        mediaitem.h
        mediaitem.cpp
        playlistdelegate.h
        playlistdelegate.cpp
        slidedeck.h
        slidedeck.cpp
        beamerwindow.h
        beamerwindow.cpp
        textslidedialog.h
        textslidedialog.cpp
        presentationconverter.h
        presentationconverter.cpp
        biblebooks.h
        biblebooks.cpp
        biblestore.h
        biblestore.cpp
        swordimporter.h
        swordimporter.cpp
        bibleimporter.h
        bibleimporter.cpp
        biblecatalog.h
        biblecatalog.cpp
        bibleinstaller.h
        bibleinstaller.cpp
        biblesettingspage.h
        biblesettingspage.cpp
        settingspage.h
        settingspage.cpp
        biblepassage.h
        biblepassage.cpp
        biblepage.h
        biblepage.cpp
        flowlayout.h
        flowlayout.cpp
        medialibrary.h
        medialibrary.cpp
        librarypage.h
        librarypage.cpp
        eventstore.h
        eventstore.cpp
        eventdialog.h
        eventdialog.cpp
        eventspage.h
        eventspage.cpp
        eventheader.h
        eventheader.cpp
        song.h
        song.cpp
        songstore.h
        songstore.cpp
        songspage.h
        songspage.cpp
        songeditor.h
        songeditor.cpp
        textmarkup.h
        textmarkup.cpp
        richtextedit.h
        richtextedit.cpp
        slidebackground.h
        slidebackground.cpp
        backgroundpicker.h
        backgroundpicker.cpp
        countdown.h
        countdown.cpp
        videothumbnailer.h
        videothumbnailer.cpp
        searchfield.h
        searchfield.cpp
        data.qrc
    )

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        APP_VERSION="${PROJECT_VERSION}"
        APP_NAME="${PROJECT_NAME}"
	COMPANY_NAME="${ORGANIZATION_NAME}"
)

# Define target properties for Android with Qt 6 as:
#    set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

# The .qm files are compiled and embedded into the app as :/i18n/<name>.qm
# ("update_translations" target: collects all tr() texts into the .ts files)
qt_add_translations(${PROJECT_NAME}
    TS_FILES ${TS_FILES}
    RESOURCE_PREFIX "/i18n"
)
else()
    if(ANDROID)
        add_library(${PROJECT_NAME} SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(${PROJECT_NAME}
            ${PROJECT_SOURCES}
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Pdf
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::WebEngineWidgets
    Qt${QT_VERSION_MAJOR}::Multimedia
    Qt${QT_VERSION_MAJOR}::MultimediaWidgets
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::CorePrivate
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER "${ORGANIZATION_DOMAIN}.${PROJECT_NAME}")
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${PROJECT_NAME})
endif()

cmake_minimum_required(VERSION 3.16)

project(Cadence VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
        src/main.cpp
        src/project.cpp
        src/project.h
        src/project.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(Cadence
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        src/engine/common/audioerror.h
        src/engine/common/audioconfig.h
        src/engine/common/audiodevice.h
        src/engine/common/audiobackend.h
        src/engine/tests/audiobackendtest.h src/engine/tests/audiobackendtest.cpp
        src/engine/backends/linux/linuxbackend.h src/engine/backends/linux/linuxbackend.cpp
        src/engine/tests/audiodevicetest.h
        src/engine/backends/rtaudiobackend.h
        src/engine/devices/rtaudiodevice.h src/engine/devices/rtaudiodevice.cpp
        src/engine/backends/rtaudiobackend.cpp
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET Cadence APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(Cadence SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(Cadence
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(Cadence PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)


target_include_directories(Cadence PRIVATE ${RT_AUDIO_INCLUDE_DIRS})
target_link_libraries(Cadence PRIVATE ${RT_AUDIO_LIBRARIES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(RT_AUDIO REQUIRED rtaudio)



# Platform-specific audio driverincludes and libraries
if(WIN32)
    target_link_libraries(Cadence PRIVATE dsound winmm)
    target_compile_definitions(Cadence PRIVATE _WIN32_WINNT=0x0601)
elseif(APPLE)
    target_link_libraries(Cadence PRIVATE
        "-framework CoreAudio"
        "-framework CoreFoundation"
    )
elseif(UNIX AND NOT APPLE)  # Linux
    target_link_libraries(Cadence PRIVATE
        asound    # ALSA
        jack      # JACK
        pthread   # Threads
    )
    if(NOT ANDROID)
        target_link_libraries(Cadence PRIVATE pulse-simple)
    endif()
endif()


# Tests
if(BUILD_TESTS)
    # Find Catch2
    find_package(Catch2 3 REQUIRED)

    # Test executable
    add_executable(AudioBackendTests
        src/engine/tests/AudioBackendTest.cpp
        src/engine/tests/AudioDeviceTest.cpp
    )

    target_link_libraries(AudioBackendTests
        PRIVATE Cadence Catch2::Catch2WithMain
    )

    # Add test
    catch_discover_tests(AudioBackendTests)
endif()








# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.Cadence)
endif()
set_target_properties(Cadence PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS Cadence
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Cadence)
endif()

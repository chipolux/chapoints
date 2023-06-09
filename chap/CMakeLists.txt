cmake_minimum_required(VERSION 3.21.0)

project(chap VERSION 1.0.0 LANGUAGES CXX)
set(APP_NAME "ChaP")
set(PROJECT_IDENTIFIER "com.nakyle.${PROJECT_NAME}")
set(ORG_NAME "nakyle.com")
set(ORG_DOMAIN "nakyle.com")

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QT_MODULES
    Core
    Network
    Qml
    Quick
    QuickControls2
    Svg
)
find_package(Qt6 COMPONENTS ${QT_MODULES} REQUIRED)
list(TRANSFORM QT_MODULES PREPEND Qt${QT_VERSION_MAJOR}::)

set(PROJECT_SOURCES
    core.cpp
    core.h
    main.cpp
    qtutils.cpp
    qtutils.h
    secrets.h
    shockcollarmanager.cpp
    shockcollarmanager.h
    smokemachinemanager.cpp
    smokemachinemanager.h
    twitchmanager.cpp
    twitchmanager.h
)
set(PROJECT_RESOURCES
    resources/Fira_Code/FiraCode-Bold.ttf
    resources/Fira_Code/FiraCode-Light.ttf
    resources/Fira_Code/FiraCode-Medium.ttf
    resources/Fira_Code/FiraCode-Regular.ttf
    resources/Fira_Code/FiraCode-Retina.ttf
    resources/Fira_Code/FiraCode-SemiBold.ttf
    resources/icon.png
    resources/icon.svg
)
file(GLOB_RECURSE PROJECT_QMLFILES RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS "*.qml")

qt_add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE ${QT_MODULES})
qt_add_qml_module(${PROJECT_NAME}
    URI ${PROJECT_NAME}
    VERSION 1.0
    SOURCES ${PROJECT_SOURCES}
    QML_FILES ${PROJECT_QMLFILES}
    RESOURCES ${PROJECT_RESOURCES}
)
qt_import_qml_plugins(${PROJECT_NAME})

# configure a header to allow C++ to access select CMake variables like
# project version, etc. it will end up in our binary directory
configure_file(config.h.in config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# warn and error for everything
if(NOT EMSCRIPTEN)
    # C4702 = Unreachable Code, Qt 6.3.2 has an annoying instance of this...
    # C4127 = Conditional is not constant, qiterable is the culprit here
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /wd4702 /wd4127>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror -Wno-comment -Wno-gnu-zero-variadic-macro-arguments>
    )
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME}
    MACOSX_BUNDLE_GUI_IDENTIFIER ${PROJECT_IDENTIFIER}
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER ${PROJECT_IDENTIFIER}
    XCODE_ATTRIBUTE_GCC_SYMBOLS_PRIVATE_EXTERN "YES"
    XCODE_ATTRIBUTE_SKIP_INSTALL "NO"
    XCODE_ATTRIBUTE_INSTALL_PATH "$(LOCAL_APPS_DIR)"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION_PATCH}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

if(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE win32/icon.rc)
endif()

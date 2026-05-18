cmake_minimum_required(VERSION 3.16)
project(DownloadTool VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 Qt6 或 Qt5，需要 Core, Widgets, Network
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Network)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    downloadtask.cpp
    downloadtask.h
    downloadworker.cpp
    downloadworker.h
    downloadmanager.cpp
    downloadmanager.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(DownloadTool MANUAL_FINALIZATION ${PROJECT_SOURCES})
else()
    add_executable(DownloadTool ${PROJECT_SOURCES})
endif()

target_link_libraries(DownloadTool PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)

# 以下为通用属性，可忽略
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.downloadtool)
endif()
set_target_properties(DownloadTool PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS DownloadTool
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(DownloadTool)
endif()
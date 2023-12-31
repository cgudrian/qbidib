cmake_minimum_required(VERSION 3.14)

project(BiDiBTest VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core SerialPort Test)
enable_testing(true)

qt_standard_project_setup(REQUIRES 6.5)

add_compile_options(-Wall -Wpedantic)

add_subdirectory(lib)

qt_add_executable(BiDiBTest
    main.cpp
)
target_include_directories(BiDiBTest PRIVATE tl)
target_link_libraries(BiDiBTest PRIVATE Qt6::SerialPort bidib)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(BiDiBTest PROPERTIES
    #    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appPainterTest
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS BiDiBTest
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

cmake_minimum_required(VERSION 3.16)
project(DBCViewer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6 first, fall back to Qt5 (cross-platform: Linux, Windows, macOS)
find_package(Qt6 QUIET COMPONENTS Core Widgets)
if(Qt6_FOUND)
    set(QT_VERSION_MAJOR 6)
else()
    find_package(Qt5 REQUIRED COMPONENTS Core Widgets)
    set(QT_VERSION_MAJOR 5)
endif()

# Enable Qt MOC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Source files
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/dbcparser.cpp
    src/canmessage.cpp
    src/cansignal.cpp
    src/dbcexcelconverter.cpp
    src/dbcwriter.cpp
    src/third_party/miniz/miniz.c
    src/third_party/miniz/miniz_tdef.c
    src/third_party/miniz/miniz_tinfl.c
    src/third_party/miniz/miniz_zip.c
)

# Header files
set(HEADERS
    src/mainwindow.h
    src/dbcparser.h
    src/cansignal.h
    src/canmessage.h
    src/dbcexcelconverter.h
    src/dbcwriter.h
)

# Create executable
add_executable(DBCViewer ${SOURCES} ${HEADERS})

# Link Qt libraries
if(QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(DBCViewer Qt6::Core Qt6::Widgets)
else()
    target_link_libraries(DBCViewer Qt5::Core Qt5::Widgets)
endif()
target_include_directories(DBCViewer PRIVATE src/third_party/miniz)

# Set target properties
set_target_properties(DBCViewer PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

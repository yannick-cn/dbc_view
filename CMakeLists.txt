cmake_minimum_required(VERSION 3.16)
project(DBCViewer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt5
find_package(Qt5 REQUIRED COMPONENTS Core Widgets)

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
)

# Header files
set(HEADERS
    src/mainwindow.h
    src/dbcparser.h
    src/cansignal.h
    src/canmessage.h
)

# Create executable
add_executable(DBCViewer ${SOURCES} ${HEADERS})

# Link Qt libraries
target_link_libraries(DBCViewer Qt5::Core Qt5::Widgets)

# Set target properties
set_target_properties(DBCViewer PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

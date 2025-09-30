cmake_minimum_required(VERSION 3.16)
project(SimpleChat VERSION 1.0.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

# Enable automatic MOC (Meta-Object Compiler)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Source files
set(SOURCES
    main.cpp
    simplechat.cpp
)

set(HEADERS
    simplechat.h
)

# Create executable
add_executable(SimpleChat ${SOURCES} ${HEADERS})

# Link Qt6 libraries
target_link_libraries(SimpleChat Qt6::Core Qt6::Widgets Qt6::Network)

# Set output directory
set_target_properties(SimpleChat PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
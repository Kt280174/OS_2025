# CMakeList.txt : CMake project for daemon, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("daemon"LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Add source to this project's executable.
add_compile_options(-Wall -Werror -O2)

file(GLOB SRC_FILES
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

add_executable(mydaemon ${SRC_FILES})

set_target_properties(mydaemon PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

#install(TARGETS mydaemon DESTINATION /usr/local/bin)

message(STATUS "Source files: ${SRC_FILES}")
message(STATUS "Output binary: ${CMAKE_BINARY_DIR}/mydaemon")


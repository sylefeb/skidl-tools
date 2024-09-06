cmake_minimum_required(VERSION 3.5)
project(skidl-updater)

INCLUDE_DIRECTORIES(
  ${PROJECT_SOURCE_DIR}/
  ${PROJECT_SOURCE_DIR}/../libs
  ${PROJECT_SOURCE_DIR}/../libs/tclap/include
  ${PROJECT_SOURCE_DIR}/../libs/LibSL-small/src
  ${PROJECT_SOURCE_DIR}/../libs/LibSL-small/src/LibSL
  ${PROJECT_SOURCE_DIR}/../libs/sexpresso/sexpresso/
)

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:4194304")
else()
if (MINGW)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --static")
endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(SKIDL_UPDATER
  main.cpp
  ../libs/sexpresso/sexpresso/sexpresso.cpp
  ../libs/LibSL-small/src/LibSL/System/System.cpp
  ../libs/LibSL-small/src/LibSL/CppHelpers/CppHelpers.cpp
  ../libs/LibSL-small/src/LibSL/StlHelpers/StlHelpers.cpp
)

# generate the git hash version string

execute_process(COMMAND git log --pretty=format:'%h' -n 1
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_VARIABLE GIT_HASH
                )

set(VERSION "const char* c_GitHash=\"${GIT_HASH}\";")
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/version.inc "${VERSION}")

# executable

add_executable(skidl-updater ${SKIDL_UPDATER})
# target_link_libraries(skidl-updater ...)

if(WIN32)
  target_link_libraries(skidl-updater shlwapi)
endif()

# install

install(TARGETS   skidl-updater         RUNTIME     DESTINATION bin/)

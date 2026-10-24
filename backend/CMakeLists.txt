cmake_minimum_required(VERSION 3.20)
project(enro LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# Root of the repo, so the binary finds contracts/, backend/config and backend/seed
# no matter where it is launched from. Override at runtime with ENRO_ROOT.
get_filename_component(ENRO_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

# ---------------------------------------------------------------- dependencies
include(FetchContent)
set(FETCHCONTENT_QUIET ON)
set(BUILD_TESTING_SAVED ${BUILD_TESTING})
set(BUILD_TESTING OFF)

# asio (standalone, header only). Crow's Findasio looks for ASIO_INCLUDE_DIR.
FetchContent_Declare(asio
  URL https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-30-2.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(asio)
set(ASIO_INCLUDE_DIR "${asio_SOURCE_DIR}/asio/include" CACHE PATH "" FORCE)

# Crow (HTTP + websockets)
set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CROW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(crow
  URL https://github.com/CrowCpp/Crow/archive/refs/tags/v1.3.4.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(crow)

# nlohmann/json
FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(json)

# cpr (HTTP client). Uses the system libcurl when present, otherwise builds curl.
find_package(CURL QUIET)
if(CURL_FOUND)
  set(CPR_USE_SYSTEM_CURL ON CACHE BOOL "" FORCE)
endif()
set(CPR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(cpr
  URL https://github.com/libcpr/cpr/archive/refs/tags/1.15.1.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(cpr)

# SQLiteCpp (bundles sqlite3)
set(SQLITECPP_RUN_CPPLINT OFF CACHE BOOL "" FORCE)
set(SQLITECPP_RUN_CPPCHECK OFF CACHE BOOL "" FORCE)
set(SQLITECPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SQLITECPP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(sqlitecpp
  URL https://github.com/SRombauts/SQLiteCpp/archive/refs/tags/3.4.0.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(sqlitecpp)

# doctest
FetchContent_Declare(doctest
  URL https://github.com/doctest/doctest/archive/refs/tags/v2.5.3.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(doctest)

set(BUILD_TESTING ${BUILD_TESTING_SAVED})

# Protobuf is optional. Only GtfsRtFeed (T15, P1) needs it. See README.
find_package(Protobuf QUIET)

# ------------------------------------------------------------------- enro_core
# Everything except main.cpp, so tests link the same code the server runs.
# New .cpp files under src/ are picked up automatically (re-run cmake).
file(GLOB_RECURSE ENRO_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
list(REMOVE_ITEM ENRO_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

add_library(enro_core STATIC ${ENRO_SOURCES})
target_include_directories(enro_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(enro_core PUBLIC ENRO_REPO_ROOT="${ENRO_REPO_ROOT}")
target_link_libraries(enro_core PUBLIC Crow::Crow nlohmann_json::nlohmann_json cpr::cpr SQLiteCpp)
if(Protobuf_FOUND)
  target_compile_definitions(enro_core PUBLIC ENRO_HAVE_PROTOBUF=1)
  target_link_libraries(enro_core PUBLIC protobuf::libprotobuf)
endif()
# CLAUDE API ALTERNATE: AnthropicClient.cpp is fully commented out, so it compiles to nothing.
# When you uncomment it, nothing else is needed here: the glob above already includes it.

add_executable(enro src/main.cpp)
target_link_libraries(enro PRIVATE enro_core)
# Put the binary at build/enro so `./build/enro` works from the repo root.
set_target_properties(enro PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# ----------------------------------------------------------------------- tests
enable_testing()
file(GLOB ENRO_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
add_executable(enro_tests ${ENRO_TEST_SOURCES})
target_link_libraries(enro_tests PRIVATE enro_core doctest::doctest)
set_target_properties(enro_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME enro_tests COMMAND enro_tests)

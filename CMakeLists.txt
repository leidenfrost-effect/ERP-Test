cmake_minimum_required(VERSION 3.20)

project(HybridPersonRepo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTING "Build unit and integration tests" ON)
option(ENABLE_STRICT_WARNINGS "Enable strict compiler warnings" ON)
option(ENABLE_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

if (WIN32)
  add_compile_definitions(_WIN32_WINNT=0x0601 WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

if (ENABLE_STRICT_WARNINGS)
  if (MSVC)
    add_compile_options(/W4)
    if (ENABLE_WARNINGS_AS_ERRORS)
      add_compile_options(/WX)
    endif()
  else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if (ENABLE_WARNINGS_AS_ERRORS)
      add_compile_options(-Werror)
    endif()
  endif()
endif()

find_package(Protobuf REQUIRED)
find_package(SQLite3 REQUIRED)

find_package(Crow CONFIG QUIET)
if (NOT Crow_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG v1.2.1
  )
  FetchContent_MakeAvailable(crow)
endif()

if (TARGET Crow::Crow)
  set(CROW_TARGET Crow::Crow)
elseif(TARGET Crow)
  set(CROW_TARGET Crow)
else()
  message(FATAL_ERROR "Crow target not found. Install Crow via vcpkg or use FetchContent.")
endif()

if (TARGET SQLite::SQLite3)
  set(SQLITE_TARGET SQLite::SQLite3)
elseif(TARGET SQLite3::SQLite3)
  set(SQLITE_TARGET SQLite3::SQLite3)
else()
  set(SQLITE_TARGET "")
endif()

add_subdirectory(proto)
add_subdirectory(core)
add_subdirectory(infra)
add_subdirectory(api)

include(CTest)
if (BUILD_TESTING)
  add_subdirectory(tests)
endif()

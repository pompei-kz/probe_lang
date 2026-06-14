cmake_minimum_required(VERSION 4.2)
project(probe_lang)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG        master
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(stb)

find_package(ICU REQUIRED COMPONENTS uc i18n io data)

find_package(SDL3 REQUIRED CONFIG)

add_executable(probe_lang
    src/main.cpp
)
target_include_directories(probe_lang PRIVATE ${stb_SOURCE_DIR})
target_link_libraries(probe_lang PRIVATE SDL3::SDL3)
target_link_libraries(probe_lang PRIVATE ICU::uc ICU::i18n ICU::io ICU::data)

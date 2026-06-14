# Invoked at build time via: cmake -DINPUT_FILE=... -DOUTPUT_FILE=... -DVAR_NAME=... -DNS=... -DIS_TEXT=... -P embed_resource.cmake
cmake_minimum_required(VERSION 3.21)

if(IS_TEXT)
    file(READ "${INPUT_FILE}" content)
    file(WRITE "${OUTPUT_FILE}" "\
#pragma once
#include <string_view>
namespace ${NS} {
inline constexpr std::string_view ${VAR_NAME} = R\"__embed__(${content})__embed__\";
} // namespace ${NS}
")
else()
    file(READ "${INPUT_FILE}" content HEX)
    string(LENGTH "${content}" hex_len)
    math(EXPR byte_count "${hex_len} / 2")
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," hex_bytes "${content}")
    file(WRITE "${OUTPUT_FILE}" "\
#pragma once
#include <cstdint>
#include <span>
namespace ${NS} {
// clang-format off
inline constexpr uint8_t ${VAR_NAME}_data[] = {${hex_bytes}};
// clang-format on
inline constexpr std::span<const uint8_t> ${VAR_NAME}{ ${VAR_NAME}_data, ${byte_count} };
} // namespace ${NS}
")
endif()

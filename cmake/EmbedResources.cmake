set(_EMBED_TEXT_EXTENSIONS
    .txt .glsl .vert .frag .comp .hlsl .json .xml .csv
    .md .ini .cfg .conf .lua .js .html .css .sql .sh
)

# embed_resources(<target> <resources_dir>)
#
# For every file in <resources_dir> generates a .S file (assembled into the target)
# and a .hpp header:
#   text files   -> std::string_view  (null terminator appended)
#   binary files -> std::span<const uint8_t>
#
# Include via:
#   #include "resources.hpp"
#   resources::SomeTextFile_txt            // std::string_view
#   resources::fonts::Roboto_Regular_ttf   // std::span<const uint8_t>
function(embed_resources target resources_dir)
    set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated_resources")

    file(GLOB_RECURSE resource_files
        CONFIGURE_DEPENDS
        RELATIVE "${resources_dir}"
        "${resources_dir}/*"
    )

    set(asm_sources "")
    set(master_includes "")

    foreach(rel_path IN LISTS resource_files)
        set(abs_path "${resources_dir}/${rel_path}")
        if(IS_DIRECTORY "${abs_path}")
            continue()
        endif()

        get_filename_component(ext "${rel_path}" LAST_EXT)
        string(TOLOWER "${ext}" ext_lower)
        list(FIND _EMBED_TEXT_EXTENSIONS "${ext_lower}" _text_idx)
        if(_text_idx GREATER_EQUAL 0)
            set(is_text TRUE)
        else()
            set(is_text FALSE)
        endif()

        get_filename_component(filename "${rel_path}" NAME)
        string(REGEX REPLACE "[^a-zA-Z0-9]" "_" var_name "${filename}")

        get_filename_component(dir_part "${rel_path}" DIRECTORY)
        if(dir_part)
            string(REPLACE "/" "::" ns_suffix "${dir_part}")
            string(REGEX REPLACE "[^a-zA-Z0-9:]" "_" ns_suffix "${ns_suffix}")
            set(namespace "resources::${ns_suffix}")
            set(out_subdir "${generated_dir}/${dir_part}")
            string(REGEX REPLACE "[^a-zA-Z0-9]" "_" path_prefix "${dir_part}")
            set(sym "_resource_${path_prefix}_${var_name}")
            list(APPEND master_includes "#include \"${dir_part}/${var_name}.hpp\"")
        else()
            set(namespace "resources")
            set(out_subdir "${generated_dir}")
            set(sym "_resource_${var_name}")
            list(APPEND master_includes "#include \"${var_name}.hpp\"")
        endif()

        file(MAKE_DIRECTORY "${out_subdir}")

        # --- .S assembly file ---
        set(asm_file "${out_subdir}/${var_name}.S")
        if(is_text)
            file(WRITE "${asm_file}" "\
        .section .rodata
        .balign 1
        .global ${sym}_start
${sym}_start:
        .incbin \"${abs_path}\"
        .byte 0
        .global ${sym}_end
${sym}_end:
        .section .note.GNU-stack,\"\",@progbits
")
        else()
            file(WRITE "${asm_file}" "\
        .section .rodata
        .balign 8
        .global ${sym}_start
${sym}_start:
        .incbin \"${abs_path}\"
        .global ${sym}_end
${sym}_end:
        .section .note.GNU-stack,\"\",@progbits
")
        endif()

        # Rebuild .o when the resource file itself changes
        set_source_files_properties("${asm_file}" PROPERTIES
            OBJECT_DEPENDS "${abs_path}"
        )
        list(APPEND asm_sources "${asm_file}")

        # --- .hpp header ---
        set(out_file "${out_subdir}/${var_name}.hpp")
        if(is_text)
            file(WRITE "${out_file}" "\
#pragma once
#include <string_view>
namespace ${namespace} {
extern \"C\" const char ${sym}_start[];
extern \"C\" const char ${sym}_end[];
// size excludes the null terminator appended by the assembler
inline std::string_view ${var_name}{ ${sym}_start, static_cast<std::size_t>(${sym}_end - ${sym}_start - 1) };
} // namespace ${namespace}
")
        else()
            file(WRITE "${out_file}" "\
#pragma once
#include <cstdint>
#include <span>
namespace ${namespace} {
extern \"C\" const uint8_t ${sym}_start[];
extern \"C\" const uint8_t ${sym}_end[];
inline std::span<const uint8_t> ${var_name}{ ${sym}_start, ${sym}_end };
} // namespace ${namespace}
")
        endif()
    endforeach()

    # Master include-all header
    string(JOIN "\n" _includes_str ${master_includes})
    file(WRITE "${generated_dir}/resources.hpp" "#pragma once\n${_includes_str}\n")

    target_sources(${target} PRIVATE ${asm_sources})
    target_include_directories(${target} PRIVATE "${generated_dir}")
endfunction()

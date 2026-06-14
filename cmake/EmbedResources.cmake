set(_EMBED_RESOURCES_DIR "${CMAKE_CURRENT_LIST_DIR}")

set(_EMBED_TEXT_EXTENSIONS
    .txt .glsl .vert .frag .comp .hlsl .json .xml .csv
    .md .ini .cfg .conf .lua .js .html .css .sql .sh
)

# embed_resources(<target> <resources_dir>)
#
# For every file in <resources_dir>:
#   text files  -> generated_resources/**/<name>.hpp  with std::string_view
#   binary files -> generated_resources/**/<name>.hpp  with std::span<const uint8_t>
#
# A master header generated_resources/resources.hpp includes all of them.
# Access via:
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

    set(all_outputs "")
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
            list(APPEND master_includes "#include \"${dir_part}/${var_name}.hpp\"")
        else()
            set(namespace "resources")
            set(out_subdir "${generated_dir}")
            list(APPEND master_includes "#include \"${var_name}.hpp\"")
        endif()

        file(MAKE_DIRECTORY "${out_subdir}")
        set(out_file "${out_subdir}/${var_name}.hpp")
        list(APPEND all_outputs "${out_file}")

        add_custom_command(
            OUTPUT "${out_file}"
            COMMAND "${CMAKE_COMMAND}"
                "-DINPUT_FILE=${abs_path}"
                "-DOUTPUT_FILE=${out_file}"
                "-DVAR_NAME=${var_name}"
                "-DNS=${namespace}"
                "-DIS_TEXT=${is_text}"
                -P "${_EMBED_RESOURCES_DIR}/embed_resource.cmake"
            DEPENDS "${abs_path}"
            COMMENT "Embedding resource: ${rel_path}"
            VERBATIM
        )
    endforeach()

    string(JOIN "\n" _includes_str ${master_includes})
    file(WRITE "${generated_dir}/resources.hpp" "#pragma once\n${_includes_str}\n")

    add_custom_target(${target}_embedded_resources DEPENDS ${all_outputs})
    add_dependencies(${target} ${target}_embedded_resources)

    target_include_directories(${target} PRIVATE "${generated_dir}")
endfunction()

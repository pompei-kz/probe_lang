cmake_minimum_required(VERSION 4.2)
project(probe_lang)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

find_package(ICU REQUIRED COMPONENTS uc i18n io data)

find_package(SDL3 REQUIRED CONFIG)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)

include(cmake/EmbedResources.cmake)

add_executable(probe_lang
    src/main.cpp
    src/common.cpp
    src/PanelMenu.cpp
    src/FontAtlas.cpp
    src/Conn.cpp
    src/ConnTest.cpp
    src/TextEditor.cpp
    src/ContextMenu.cpp
    src/InputField.cpp
    src/Dlg.cpp
    src/MsgDlg.cpp
    src/ConfirmDlg.cpp
    src/SchemaMenu.cpp
    src/FolderMenu.cpp
    src/SelectableText.cpp
    src/FormEditRepository.cpp
    src/FormEditFolder.cpp
    src/CustomId.cpp
)
target_include_directories(probe_lang PRIVATE ${stb_SOURCE_DIR})
target_link_libraries(probe_lang PRIVATE SDL3::SDL3)
target_link_libraries(probe_lang PRIVATE ICU::uc ICU::i18n ICU::io ICU::data)
target_include_directories(probe_lang PRIVATE ${PQXX_INCLUDE_DIRS})
target_link_libraries(probe_lang PRIVATE ${PQXX_LINK_LIBRARIES})

embed_resources(probe_lang "${CMAKE_CURRENT_SOURCE_DIR}/resources")
target_include_directories(probe_lang PRIVATE external/stb)

# ── Tests ────────────────────────────────────────────────────────────────────
enable_testing()

find_package(GTest REQUIRED)

add_executable(probe_lang_tests
    src_test/TestCustomId.cpp
    src/CustomId.cpp
)
target_include_directories(probe_lang_tests PRIVATE src)
target_link_libraries(probe_lang_tests PRIVATE GTest::GTest GTest::Main)

include(GoogleTest)
gtest_discover_tests(probe_lang_tests)

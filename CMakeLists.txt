cmake_minimum_required(VERSION 4.2)
project(probe_lang)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

find_package(ICU REQUIRED COMPONENTS uc i18n io data)

find_package(SDL3 REQUIRED CONFIG)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)
# libpqxx 8 may be installed as a static archive whose .pc only lists libpq under
# Requires.private, so the C client library is not pulled in automatically — link it
# explicitly (after libpqxx, for static archive symbol resolution).
pkg_check_modules(PQ REQUIRED libpq)

include(cmake/EmbedResources.cmake)

add_executable(probe_lang
        src/main.cpp
        src/front/AppIcon.cpp
        src/front/common.cpp
        src/front/PanelMenu.cpp
        src/front/FontAtlas.cpp
        src/front/TextEditor.cpp
        src/front/ContextMenu.cpp
        src/front/InputField.cpp
        src/front/Dlg.cpp
        src/front/MsgDlg.cpp
        src/front/ConfirmDlg.cpp
        src/front/SchemaMenu.cpp
        src/front/FolderMenu.cpp
        src/front/SelectableText.cpp
        src/front/FormEditRepository.cpp
        src/front/FormEditFolder.cpp
        src/front/FormEditUnit.cpp
        src/front/FormSelectUnit.cpp
        src/front/UnitMenu.cpp
        src/front/ContextMenuSelExpr.cpp
        src/front/EditorView.cpp
        src/back/ConnService.cpp
        src/back/RepoService.cpp
        src/back/FolderService.cpp
        src/back/InitDb.cpp
        src/back/UtilDb.cpp
        src/back/ProjectTreeService.cpp
        src/back/UnitService.cpp
        src/back/BlockService.cpp
        src/back/ExprService.cpp
        src/back/UnitEditorState.cpp
        src/back/CustomId.cpp
        src/back/model/RowChange.h
        src/back/model/ExprType.h
        src/back/ChangeSystem.cpp
        src/back/ChangeSystem.h
        src/back/model/ChangeSysTarget.h
        src/back/model/ChangeOp.h
)
target_include_directories(probe_lang PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(probe_lang PRIVATE ${stb_SOURCE_DIR})
target_link_libraries(probe_lang PRIVATE SDL3::SDL3)
target_link_libraries(probe_lang PRIVATE ICU::uc ICU::i18n ICU::io ICU::data)
target_include_directories(probe_lang PRIVATE ${PQXX_INCLUDE_DIRS})
target_link_libraries(probe_lang PRIVATE ${PQXX_LINK_LIBRARIES} ${PQ_LINK_LIBRARIES})

embed_resources(probe_lang "${CMAKE_CURRENT_SOURCE_DIR}/resources")
target_include_directories(probe_lang PRIVATE external/stb)

# ── Tests ────────────────────────────────────────────────────────────────────
enable_testing()

find_package(GTest REQUIRED)

add_executable(probe_lang_tests
        src_test/back/TestCustomId.cpp
        src_test/back/TestConnService.cpp
        src_test/back/TestUtilDb.cpp
        src_test/back/TestInitDb.cpp
        src_test/back/TestRepoService.cpp
        src_test/back/TestFolderService.cpp
        src_test/back/TestProjectTreeService.cpp
        src_test/back/TestUnitService.cpp
        src_test/back/TestBlockService.cpp
        src_test/back/TestExprService.cpp
        src_test/back/TestChangeSystem.cpp
        src_test/back/TestUnitEditorState.cpp
        src/back/CustomId.cpp
        src/back/ConnService.cpp
        src/back/UtilDb.cpp
        src/back/InitDb.cpp
        src/back/RepoService.cpp
        src/back/FolderService.cpp
        src/back/ProjectTreeService.cpp
        src/back/UnitService.cpp
        src/back/BlockService.cpp
        src/back/ExprService.cpp
        src/back/ChangeSystem.cpp
        src/back/UnitEditorState.cpp
        src/back/model/RowChange.h
        src/back/model/ExprType.h
)
target_include_directories(probe_lang_tests PRIVATE src)
target_include_directories(probe_lang_tests PRIVATE src_test)
target_include_directories(probe_lang_tests PRIVATE ${PQXX_INCLUDE_DIRS})
target_link_libraries(probe_lang_tests PRIVATE GTest::GTest GTest::Main)
target_link_libraries(probe_lang_tests PRIVATE ${PQXX_LINK_LIBRARIES} ${PQ_LINK_LIBRARIES})

include(GoogleTest)
gtest_discover_tests(probe_lang_tests)

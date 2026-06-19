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
        src/back/service/ConnServiceR.cpp
        src/back/service/ConnServiceRW.cpp
        src/back/service/RepoServiceR.cpp
        src/back/service/RepoServiceRW.cpp
        src/back/service/FolderServiceR.cpp
        src/back/service/FolderServiceRW.cpp
        src/back/etc/InitDb.cpp
        src/back/etc/UtilDb.cpp
        src/back/service/ProjectTreeServiceR.cpp
        src/back/service/ProjectTreeServiceRW.cpp
        src/back/service/UnitServiceR.cpp
        src/back/service/UnitServiceRW.cpp
        src/back/service/BlockServiceR.cpp
        src/back/service/BlockServiceRW.cpp
        src/back/service/ExprServiceR.cpp
        src/back/service/ExprServiceRW.cpp
        src/back/etc/UnitEditorState.cpp
        src/back/etc/CustomId.cpp
        src/back/model/RowChange.h
        src/back/model/ExprType.h
        src/back/etc/ChangeSystem.cpp
        src/back/etc/ChangeSystem.h
        src/back/model/ChangeSysTarget.h
        src/back/model/ChangeOp.h
        src/back/model/Conn.cpp
        src/back/model/Conn.h
        src/back/model/ConnHash.h
        src/back/pool/Connection.cpp
        src/back/pool/Connection.h
        src/back/pool/Pool.cpp
        src/back/pool/Pool.h
        src/back/pool/PoolManager.cpp
        src/back/pool/PoolManager.h
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
        src_test/back/etc/TestCustomId.cpp
        src_test/back/service/TestConnServiceR.cpp
        src_test/back/service/TestConnServiceRW.cpp
        src_test/back/etc/TestUtilDb.cpp
        src_test/back/etc/TestInitDb.cpp
        src_test/back/service/TestRepoServiceR.cpp
        src_test/back/service/TestRepoServiceRW.cpp
        src_test/back/service/TestFolderServiceR.cpp
        src_test/back/service/TestFolderServiceRW.cpp
        src_test/back/service/TestProjectTreeServiceR.cpp
        src_test/back/service/TestProjectTreeServiceRW.cpp
        src_test/back/service/TestUnitServiceR.cpp
        src_test/back/service/TestUnitServiceRW.cpp
        src_test/back/service/TestBlockServiceR.cpp
        src_test/back/service/TestBlockServiceRW.cpp
        src_test/back/service/TestExprServiceR.cpp
        src_test/back/service/TestExprServiceRW.cpp
        src_test/back/etc/TestChangeSystem.cpp
        src_test/back/etc/TestUnitEditorState.cpp
        src/back/etc/CustomId.cpp
        src/back/service/ConnServiceR.cpp
        src/back/service/ConnServiceRW.cpp
        src/back/etc/UtilDb.cpp
        src/back/etc/InitDb.cpp
        src/back/service/RepoServiceR.cpp
        src/back/service/RepoServiceRW.cpp
        src/back/service/FolderServiceR.cpp
        src/back/service/FolderServiceRW.cpp
        src/back/service/ProjectTreeServiceR.cpp
        src/back/service/ProjectTreeServiceRW.cpp
        src/back/service/UnitServiceR.cpp
        src/back/service/UnitServiceRW.cpp
        src/back/service/BlockServiceR.cpp
        src/back/service/BlockServiceRW.cpp
        src/back/service/ExprServiceR.cpp
        src/back/service/ExprServiceRW.cpp
        src/back/etc/ChangeSystem.cpp
        src/back/etc/UnitEditorState.cpp
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

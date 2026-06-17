# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

`probe_lang` is a desktop visual-programming editor (C++26 + SDL3). The left pane (30% width) is a
tree of **database connections → repositories → folders → units**; the right pane is a tabbed
graphical canvas where a unit's **blocks** (methods/fields) are drawn as boxes you can pan, zoom,
drag, and edit inline. Each *repository* is a PostgreSQL **schema**; blocks are stored with a PostGIS
`geom` column so the visible viewport is fetched with a spatial (GIST `&&`) query.

Code comments and docstrings are bilingual (Russian + English); match the surrounding language when
editing a given file.

## Build / run / test

Everything goes through the `Makefile`, which wraps the CMake `debug-clang` preset (Ninja, clang-23,
build dir `cmake-build-debug-clang/`):

```bash
make build      # configure + compile
make run        # build, then launch the GUI app
make test       # build, then run ctest (GoogleTest, output-on-failure)
make rebuild    # wipe build dir, reconfigure, build, test
make clean
```

Run a single test (binary is `cmake-build-debug-clang/probe_lang_tests`):

```bash
ctest --test-dir cmake-build-debug-clang -R TestUnitService --output-on-failure
./cmake-build-debug-clang/probe_lang_tests --gtest_filter='UnitServiceTest.*'
```

System dependencies (see `installs.txt`): `libicu-dev`, `libpq-dev libpqxx-dev`, `libgtest-dev`,
plus SDL3 (built from source — see `install-SDL3-from-src/`). The `external/stb` submodule must be
checked out (`git submodule update --init`).

## Test database

Backend tests run against a **real PostgreSQL+PostGIS** instance, not a mock. Bring it up with Docker:

```bash
bash docker/docker-restart.bash    # starts pg (app, :18401) and pg-test (:18402)
```

- `src_test/TestDb.h` is the single source of truth for the test DSN (`localhost:18402`,
  `probe_lang_test_db_01`). Override with the `PROBE_LANG_TEST_DSN` env var for CI.
- `src_test/back/DbTestBase.h` (`DbTest` fixture): each test gets its **own** schema named
  `<TestName>_<microsecond-timestamp>`, and schemas are **deliberately not dropped** so failures can
  be inspected. If the DB is unreachable the test calls `GTEST_SKIP()` rather than failing.

### Test layout convention (follow it in every new test)

Each test is arrange / act / assert, with the **act** — the single call under test — fenced by two
`//` comment lines above and below, blanks around the fence, and **no assertions inside the fence**.
Setup goes above the fence; all `ASSERT_*`/`EXPECT_*` go below it:

```cpp
TEST_F(BlockServiceTest, DeleteBlockRemovesField)
{
  make_schema();
  auto [fid, fmsg] = create_block(conn(), schema, "u1", BlockType::Field, 0, 0, 50, 20, "f");
  ASSERT_FALSE(fid.empty()) << fmsg;

  //
  //
  auto del = delete_block(conn(), schema, fid, BlockType::Field);
  //
  //

  ASSERT_TRUE(del.first) << del.second;
  auto [blocks, err] = load_blocks_in_view(conn(), schema, "u1", -1000, -1000, 1000, 1000);
  ASSERT_TRUE(err.empty()) << err;
  EXPECT_TRUE(blocks.empty());
}
```

## Architecture

Two namespaces, cleanly separated — `back` (logic + persistence) never includes `front`.

### `src/back` — services & model

- `model/` holds plain structs (`Conn`, `RepoNode`, `FolderNode`, `Unit`, `Block`, `BBox`, …) plus
  free `to_string` / `*_from_string` enum converters. No behavior.
- Each `*Service.{h,cpp}` is a set of **free functions** (no service classes). The pervasive
  convention: operations return `std::pair<bool, std::string>` = `{ok, error_message}`, or
  `std::pair<T, std::string>` where an empty/`nullopt` first element signals failure. Callers surface
  the error string in a `MsgDlg`.
- `InitDb` / `UtilDb` hold **idempotent DDL** (`CREATE ... IF NOT EXISTS`, `ensureCreatedAt`,
  `ensureLastModifiedAt`). Schemas/tables are created lazily — `ensure_*` is called when a branch is
  opened or a connection becomes "connected", so repositories predating a newer table get it on next
  open. When adding a table/column, extend the relevant `ensure_*` path rather than assuming it exists.
- `CustomId` generates time+random IDs (base-64 alphabet, no DB sequences).

### `src/front` — SDL UI

- `App` (`front/App.h`) is the single mutable state bag: the renderer, the connection tree
  (`conns`), every dialog/menu instance, and `pending_delete_*` fields used to defer destructive
  actions until a `ConfirmDlg` is answered.
- `main.cpp` is one big **immediate-mode event+render loop**: poll SDL events, route them to whichever
  dialog/menu/editor is open (dialog precedence is hard-coded via `any_dlg()`), then redraw
  everything each frame. There is no retained widget tree — widgets are structs (`InputField`,
  `ContextMenu`, `EditorView`, the `*Menu` and `*Dlg` types) whose `render(...)` both draws and
  returns a result code (`1` = confirmed, `-1` = cancelled, `0` = still open).
- `EditorView` owns `EditorTab`s (one open unit each, with independent pan/zoom camera). It calls
  `BlockService::load_blocks_in_view` on the active tab's viewport.

### Persistence outside the DB

- **Connections** are stored as files under a workspace dir (`back::ws_dir()`), not in Postgres.
- **Tree open/closed state**: empty marker files in `~/.config/probe_lang/project_tree_open_nodes/`,
  named by the `#`-joined branch-id path (`ProjectTreeService`).
- **Per-unit editor camera** (zoom/offset): one `key=value` file per unit id under
  `~/.config/probe_lang/unit_editor_sys_coord/` (`UnitEditorState`).

### Resources

`cmake/EmbedResources.cmake` compiles everything under `resources/` into the binary at build time.
Access embedded data via `#include "resources.hpp"`: text files become `std::string_view`
(`resources::SomeTextFile_txt`), binaries become `std::span<const uint8_t>`
(`resources::fonts::Roboto_Regular_ttf`). Fonts are rasterized with stb into a `FontAtlas`.

## DB schema shape (per repository schema)

`lang_setting`, `folder` (self-referential tree via `parent_id`), `unit` (Class/Interface/Enum,
optional `parent_folder_id`), `unit_bl` (a block: `x/y/width/height` + generated PostGIS `geom`,
GIST-indexed), and the detail tables `unit_bl_method` / `unit_bl_field` (1:1 with `unit_bl`, chosen by
`BlockType`) plus `unit_bl_method_arg` (ordered method arguments). Block geometry edits update
`x/y/width/height`; `geom` regenerates automatically.

## Conventions

- `.clang-format` (LLVM base, 2-space indent, 150 col, aligned consecutive assignments/declarations,
  custom brace wrapping). Run it on touched files.
- The project is developed in CLion; `// ReSharper disable once ...` comments are intentional — leave
  them in place.

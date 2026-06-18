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

#### Expressions on the canvas

When `unit_b_field.expr_id_used` is TRUE, the field block draws its **expression** to the right of
the name (instead of the "Размер: N байт" message):
- no expression (soft ref `expr_id` absent) → an empty square sized like the field's "F" glyph, with
  a little cube (≈⅓ of the square) drawn inside — this is the affordance that opens **ContextMenuSelExpr**;
- `ExprType::ThisObject` / `ThisUnit` / `ThisMethod` → the literal text "Этот Объект" / "Этот Юнит" /
  "Этот Метод";
- `ExprType::Unit` → a diamond emblem, then the referenced `unit.name`; if the soft ref
  `unit_e_unit.unit_id -> unit.id` is absent, "ОШИБКА: Юнит не существует" in red instead.

**ContextMenuSelExpr** (`Контекстное меню выбора выражения`) is a **multi-level** context menu opened
on the cube square, used to choose what an expression is. It will keep growing; current items:
- `[1] "Этот"` → submenu: `[2] "Объект"` / `[2] "Юнит"` / `[2] "Метод"` set `unit_e.type` to
  `ThisObject` / `ThisUnit` / `ThisMethod`;
- `[1] "Юнит"` → opens the **unit-selection form** (a filtered, lazily-paginated, scrollable list of
  units; picking one sets `unit_e.type = 'Unit'` and `unit_e_unit.unit_id = <picked unit id>`). The
  list appends pages as the scrollbar nears the end and shows a dim "(Всё, данных больше нет)" once
  exhausted; the filter matches units whose name contains the filter's words in order.

(`[N]` marks the menu level an item appears on.)

### Persistence outside the DB

- **Connections** are stored as files under a workspace dir (`back::ws_dir()`), not in Postgres.
- **Tree open/closed state**: empty marker files in `~/.config/probe_lang/project_tree_open_nodes/`,
  named by the `#`-joined branch-id path (`ProjectTreeService`).
- **Per-unit editor camera** (zoom/offset): one `key=value` file per unit id under
  `~/.config/probe_lang/unit_editor_sys_coord/` (`UnitEditorState`).

### Resources

`cmake/EmbedResources.cmake` compiles everything under `resources/` into the binary at build time
(via `.incbin` in a generated `.S` per file — no codegen step, no filesystem reads at runtime).
Access embedded data via `#include "resources.hpp"`: text files become `std::string_view`
(`resources::SomeTextFile_txt`), binaries become `std::span<const uint8_t>`
(`resources::fonts::Roboto_Regular_ttf`). Fonts are rasterized with stb into a `FontAtlas`; the
window icon (`resources/application_icon.png`) is decoded with stb_image in `front/AppIcon.cpp`.

### Adding a resource

1. **Drop the file under `resources/`** (subdirectories are kept as nested namespaces — e.g.
   `resources/fonts/Foo.ttf` → `resources::fonts::Foo_ttf`). The accessor symbol is the file name
   with every non-alphanumeric character replaced by `_` (so `application_icon.png` →
   `resources::application_icon_png`, extension included).
2. **Text vs binary is decided purely by extension**, from the `_EMBED_TEXT_EXTENSIONS` list at the
   top of `cmake/EmbedResources.cmake` (`.txt .json .sql .md .glsl …`):
   - **Text** → `std::string_view` with a trailing `\0` appended (the `\0` is excluded from
     `.size()`), so it is safe to pass to C string APIs.
   - **Binary** (anything *not* in that list — `.png .ttf .ico …`) → `std::span<const uint8_t>`.
   To embed a new **text** kind, add its extension to `_EMBED_TEXT_EXTENSIONS`; for a **binary**
   resource do nothing — any unlisted extension is binary by default.
3. **Reconfigure** so the new file is picked up: `make build` re-runs CMake (the glob is
   `CONFIGURE_DEPENDS`), regenerating the `.S`/`.hpp` and `resources.hpp`. New top-level source
   files (`.cpp`) must also be added to the `add_executable`/test lists in `CMakeLists.txt` — sources
   are listed explicitly, not globbed.
4. **Use it**: `#include "resources.hpp"`, then read the span/string_view. Decode binaries in code
   (e.g. stb_image for images — see `front/AppIcon.cpp` for the PNG → `SDL_Surface` pattern).

## DB schema shape (per repository schema)

`lang_setting`, `folder` (self-referential tree via `parent_id`), `unit` (Class/Interface/Enum,
optional `parent_folder_id`), `unit_b` (a block: `x/y/width/height` + generated PostGIS `geom`,
GIST-indexed), and the detail tables `unit_b_method` / `unit_b_field` (1:1 with `unit_b`, chosen by
`BlockType`) plus `unit_b_method_arg` (ordered method arguments). Block geometry edits update
`x/y/width/height`; `geom` regenerates automatically.

**Expressions** live in `unit_e` (base: `type` per `model::ExprType` — `ThisObject` / `ThisUnit` /
`ThisMethod` / `Unit` — plus `x/y/width/height` + generated `geom`) and per-type detail tables
`unit_e_{some}` (e.g. `unit_e_unit` for an `ExprType::Unit`, holding `unit_id`). Every
`unit_e_{some}.id` is a **soft reference** to `unit_e.id` (see below). A field's type can be given by
an expression: `unit_b_field.expr_id` is a soft reference to `unit_e.id`, used only when
`unit_b_field.expr_id_used` is TRUE.

## Soft references (мягкая связанность)

A **soft reference** is a nullable id column `DSA.asd_id` pointing at `ASD.id` that is **not** a DB
foreign key and is resolved leniently:

- `DSA.asd_id IS NULL` → the reference is absent. This is a **normal** state, not an error.
- `DSA.asd_id` holds an id that does **not** exist in `ASD.id` → still **not** an error; treat it
  exactly as if it were NULL (reference absent). This is a **normal** state.
- The reference `DSA.asd_id -> ASD.id` is considered present/working **only** when the id actually
  exists in `ASD.id`.

So code that follows a soft reference must `LEFT JOIN` (never assume the row exists) and behave
identically for "NULL" and "dangling id". All `unit_e_{some}.id -> unit_e.id` links follow this rule,
as does `unit_b_field.expr_id -> unit_e.id` and `unit_e_unit.unit_id -> unit.id`.

## Conventions

- `.clang-format` (LLVM base, 2-space indent, 150 col, aligned consecutive assignments/declarations,
  custom brace wrapping). Run it on touched files.
- The project is developed in CLion; `// ReSharper disable once ...` comments are intentional — leave
  them in place.
- **One type per file**: every `enum`, `struct` and `class` lives in its own header named exactly after
  it (e.g. `BlockType` → `model/BlockType.h`, `InitDb` → `back/InitDb.h`). An aggregate type that owns
  others (e.g. `Block`) just `#include`s their headers. Its free `to_string` / `*_from_string` converters
  stay in the same file as the type they convert.

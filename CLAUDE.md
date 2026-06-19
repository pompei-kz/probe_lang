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

`back/` itself contains **only subfolders** — `model/`, `service/`, `etc/`, `pool/`:

- `model/` holds plain structs (`ConnStore`, `Conn`, `RepoNode`, `FolderNode`, `Unit`, `Block`,
  `BBox`, …) plus free `to_string` / `*_from_string` enum converters. Two connection structs, easy to
  mix up: **`ConnStore`** is a *saved* database connection — `name` + host/port/user/pass/dbname + a
  `connected` flag — persisted as files (see "Persistence outside the DB") and passed to the service
  functions; **`Conn`** is just the minimal subset needed to *open* a connection
  (host/port/user/pass/dbname), produced by `ConnStore::conn()`. `Conn` has value identity
  (defaulted `operator<=>` + a `ConnHash` functor) so it can key a connection lookup.
- `service/` holds the services as **free functions** (no service classes). Each service is **split by
  read vs write**: `Foo` has a read-only `FooServiceR.{h,cpp}` (only reads the DB / filesystem) and a
  read-write `FooServiceRW.{h,cpp}` (creates / updates / deletes). The seven services are `Block`,
  `Conn`, `Expr`, `Folder`, `ProjectTree`, `Repo`, `Unit`. Everything stays in namespace `back` with
  unchanged function names — a caller just includes the R and/or RW header(s) it needs. The pervasive
  convention: operations return `std::pair<bool, std::string>` = `{ok, error_message}`, or
  `std::pair<T, std::string>` where an empty/`nullopt` first element signals failure. Callers surface
  the error string in a `MsgDlg`.
  - Each DB operation acquires its `pqxx::connection` from the **connection pool** (see below), not a
    fresh connect — `pool::Connection pgPool = pool::acquire(c); pqxx::connection &pg = *pgPool;` — so
    the connection returns to the pool when the function exits.
  - The pure helpers `make_cs` (`Conn` → libpq connection string) and `sql_err_msg` (format a
    `pqxx::sql_error`) live in **`RepoServiceR`**; every service `.cpp` that needs them includes
    `back/service/RepoServiceR.h`. `RepoServiceR` also owns the cross-service readers `load_tree` /
    `load_folders_for_schema` / `load_units_for_schema` it composes from `FolderServiceR` /
    `UnitServiceR`.
- `etc/` holds the rest of `back` that isn't a service: `InitDb`, `UtilDb`, `CustomId`,
  `UnitEditorState`, `ChangeSystem`.
- `InitDb` / `UtilDb` (`back/etc/`) hold **idempotent DDL** (`CREATE ... IF NOT EXISTS`,
  `ensureCreatedAt`, `ensureLastModifiedAt`). Schemas/tables are created lazily — `ensure_*` is called
  when a branch is opened or a connection becomes "connected", so repositories predating a newer table
  get it on next open. When adding a table/column, extend the relevant `ensure_*` path rather than
  assuming it exists.
- `CustomId` (`back/etc/`) generates time+random IDs (base-64 alphabet, no DB sequences).
- `pool/` holds the DB **connection pool** (`back::pool`, see below).

Include paths are root-relative from `src/`: back headers are included as `back/service/Foo.h`,
`back/etc/Foo.h`, or `back/model/Foo.h` (everywhere, including from within `back` itself).

#### Connection pool (`back::pool`)

Postgres connection setup is ~15 ms (TCP + backend fork + SCRAM auth), so DB connections are **pooled
and reused** instead of opened per call. Keyed by `model::Conn` (connection identity) via `ConnHash`:

- **`Pool`** — one per `Conn`. `acquire()` hands out an idle `pqxx::connection` (or opens one, up to a
  max, blocking on a `condition_variable` when the cap is reached); validates `is_open()` and discards
  stale ones. **`Connection`** is the RAII handle returned by `acquire()` — on destruction it returns
  the underlying connection to its pool (`release()`), or drops it if broken. Use `*conn` / `conn->` to
  reach the `pqxx::connection`.
- **`PoolManager`** — owns the `Conn → shared_ptr<Pool>` map. Outstanding `Connection`s keep their
  `Pool` alive (shared_ptr) even after the manager drops it, so in-flight work always finishes cleanly.
- **`PoolService`** (`back::pool::{manager, acquire, closeConnectionPool, closeAllConnectionPools}`) —
  the single process-wide `PoolManager` (a Meyers singleton). **Services call `pool::acquire(key)`**;
  `make_cs` is invoked *inside* the pool, not by callers. (`test_connection` is the one exception — it
  connects directly to validate not-yet-saved credentials.)

Lifecycle is driven from the **front**: `main.cpp` calls `pool::closeAllConnectionPools()` on shutdown;
disconnecting or deleting a connection in the project tree calls `pool::closeConnectionPool(conn.conn())`
to drop that pool.

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

Blocks and expressions are **drawn and queried independently**. `EditorView::reload()` runs two
viewport spatial queries for the active tab: `load_blocks_in_view` (blocks) and `load_exprs_in_view`
(expressions) — each returns only rows whose `geom` intersects the visible rectangle.

When `unit_b_field.expr_id_used` is TRUE the field reserves an **expression slot** —
`unit_b_field.expr_{x,y,width,height}`, a rectangle **relative to the block's `unit_b.{x,y}`** — and
the block is laid out/drawn around it. The block itself only draws, in that slot, the
**empty-expression cube** (an outlined square with a ⅓-size cube inside) when there is no expression
yet (soft ref `expr_id` absent); the cube is the affordance that opens **ContextMenuSelExpr**.

The expression itself is a separate `unit_e` entity drawn by its own pass at its **world** rectangle
`unit_e.{x,y,width,height}` (which is kept equal to the block's position plus the slot):
- `ExprType::ThisObject` / `ThisUnit` / `ThisMethod` → the literal text "Этот Объект" / "Этот Юнит" /
  "Этот Метод";
- `ExprType::Unit` → a diamond emblem, then the referenced `unit.name`; if the soft ref
  `unit_e_unit.unit_id -> unit.id` is absent, "ОШИБКА: Юнит не существует" in red instead.

These two coordinate stores are **recomputed on save**: `update_field_expr_rect` writes the field slot
and syncs `unit_e.{x,y,width,height}`; moving a block (`update_block_position`) drags its expression
along (`unit_e.{x,y} = block.{x,y} + slot`). `EditorView::refit_field` recomputes the slot after any
change to a field's expression.

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

- **Connections** (`ConnStore` records) are stored as files under a workspace dir (`back::ws_dir()`),
  not in Postgres.
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

## Change System (система изменений — undo/redo)

A generic, table-agnostic undo/redo log that records edits to **any** table (one with a `VARCHAR`
primary key named `id`) and can roll them back or replay them. Lives in `back::ChangeSystem`
(`back/etc/ChangeSystem.{h,cpp}`) — a class constructed per call with `(pqxx::work &txn, pqxx::connection
&pg, const std::string &schema)`; all of its work happens inside the caller's transaction. Its three
DB tables are created lazily, per repository schema, by `InitDb::init_change_system_tables()`.

### The unit of change: `model::RowChange`

A `RowChange` (`back/model/RowChange.h`) describes one edit to the row `id == idValue` of table
`tableName`:
- `toDelete == true` → delete that row.
- `toDelete == false` → **upsert** `colName = value` (`INSERT … ON CONFLICT (id) DO UPDATE`), so it
  creates the row if absent. `value` is `std::optional<std::string>`; `nullopt` means SQL `NULL`. The
  text is cast to the column's real type by PostgreSQL, so non-text columns work.

A logical edit is a `std::vector<RowChange>`. Recreating a row is expressed as one `RowChange` per
non-`id` column (the first upsert creates the row, the rest fill columns in). **Limitation:** because
that recreation is column-by-column, a row with a `NOT NULL` non-`id` column cannot be rebuilt this
way (an intermediate upsert would violate the constraint).

### Tables (per repository schema)

- **`undo_buffer`** — one per edit *target*: `id`, `target_id`, `target_type`, `updated_at`. A target
  is `model::ChangeSysTarget {targetId, targetType}` (currently `target_type` is always `'Unit'`).
- **`undo_op`** — one *operation* (a logged edit) within a buffer: `id`, `undo_buffer_id`,
  `order_index` (`BIGINT`, from sequence `undo_op_seq` — defines operation order), `undone` (`BOOL`),
  `group_name`, `name`. The op's `name`/`group_name` come from `model::ChangeOp {operation, group}`.
- **`undo_row_change`** — the individual `RowChange`s of an op: `id`, `undo_op_id`, `table_name`,
  `id_value`, `to_delete`, `col_name`, `col_value`, and `direction TEXT CHECK (direction IN
  ('Forward','ForUndo'))`. **`Forward`** rows are exactly what the user did; **`ForUndo`** rows are
  the changes that fully revert it.

### The stack model

Within a buffer, ops are ordered by `undo_op.order_index`. The **done** ops (`undone = FALSE`) form a
prefix and the **undone** ops (`undone = TRUE`, available for redo) form the suffix:

- **Active operation** (the one undo will revert) = the *last* op with `undone = FALSE` (max
  `order_index`). There is **no** `order_index` on `undo_buffer` — the active op is found purely via
  `undone`.
- **Next redo operation** = the *first* op with `undone = TRUE` (min `order_index`).

### Methods

- **`collectUndoChanges(userChanges)`** — pure helper, no writes. Called **before** `userChanges` are
  applied; reads each affected row's current state from the DB and returns the `RowChange`s that
  restore it, in **reverse** order. An update→ undo restores the old `colName` value (or deletes the
  row if it didn't exist yet); a delete → undo emits one set-column change per non-`id` column.
- **`apply(userChanges, operation, target)`** — the only entry point that mutates data. It: finds or
  creates the target's buffer; **wipes the redo stack** (deletes all `undone = TRUE` ops of the buffer
  and their `undo_row_change`s — a new action makes redo impossible); calls `collectUndoChanges`
  *before* applying; inserts a new `undo_op` (`undone = FALSE`); stores the user changes as `Forward`
  and the collected reverse changes as `ForUndo`; applies the user changes to the real tables; bumps
  `undo_buffer.updated_at`.
- **`undo(targetId, grouped) -> bool`** — applies the active op's `ForUndo` changes and sets its
  `undone = TRUE`. `grouped == true` undoes the whole contiguous run of newest ops sharing the active
  op's `group_name` (stops at the first different group). Returns `false` if there is no buffer or
  nothing to undo.
- **`redo(targetId, grouped) -> bool`** — symmetric: applies the next-redo op's `Forward` changes and
  sets `undone = FALSE`; `grouped` replays the contiguous same-group run from the oldest undone op.
  Returns `false` if there is nothing to redo.

Order **between** ops matters and is honored — undo runs newest→oldest, redo oldest→newest. Order
**within** a single op does not affect the final state (each change targets a distinct cell), though
it is fixed by `ORDER BY id` for determinism. `undo`/`redo` resolve the buffer by `target_id` alone.

## Conventions

- `.clang-format` (LLVM base, 2-space indent, 150 col, aligned consecutive assignments/declarations,
  custom brace wrapping). Run it on touched files.
- The project is developed in CLion; `// ReSharper disable once ...` comments are intentional — leave
  them in place.
- **One type per file**: every `enum`, `struct` and `class` lives in its own header named exactly after
  it (e.g. `BlockType` → `model/BlockType.h`, `InitDb` → `back/etc/InitDb.h`). An aggregate type that owns
  others (e.g. `Block`) just `#include`s their headers. Its free `to_string` / `*_from_string` converters
  stay in the same file as the type they convert.

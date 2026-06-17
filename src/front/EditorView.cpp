#include "EditorView.h"
#include "Clr.h"
#include "FontAtlas.h"
#include "back/BlockService.h"
#include "back/UnitEditorState.h"
#include "render_helpers.h"

#include <algorithm>
#include <cmath>

namespace front {

  using namespace back;
  using namespace back::model;

  static constexpr float ZOOM_MIN = 0.15f, ZOOM_MAX = 8.f;
  static constexpr float GRID  = 50.f;
  static constexpr float PAD   = 5.f;  // inner padding (left == top == bottom)
  static constexpr float GAP   = 6.f;  // gap between the badge and the name
  static constexpr float BADGE = 22.f; // M/F badge size (unchanged)
  // Compact box height: the badge plus equal top/bottom padding (== PAD == left).
  static constexpr float BOX_H = BADGE + 2.f * PAD;
  static constexpr float TAB_H = 30.f;
  // Below a method's header come the "add argument" (+) row and one row per
  // argument; ROW_H is the (world-unit) height of each, PLUS_D the +-circle.
  static constexpr float ROW_H  = FS + 6.f;
  static constexpr float PLUS_D = 9.f;

  // ── unit-type glyph (mirrors the tree's draw_unit_icon) ──────────────────────
  static void draw_unit_icon(SDL_Renderer *r, float x, float yc, UnitType t, Clr c)
  {
    sc(r, c);
    switch (t) {
      case UnitType::Class: {
        SDL_FRect b{x, yc - 4.f, 9.f, 9.f};
        SDL_RenderFillRect(r, &b);
        break;
      }
      case UnitType::Interface: {
        SDL_FRect b{x, yc - 4.f, 9.f, 9.f};
        SDL_RenderRect(r, &b);
        break;
      }
      case UnitType::Enum: {
        for (int i = 0; i < 3; i++) {
          SDL_FRect bar{x, yc - 4.f + i * 3.5f, 9.f, 1.6f};
          SDL_RenderFillRect(r, &bar);
        }
        break;
      }
    }
  }

  static float tab_width(const EditorTab &t)
  {
    return std::clamp(10.f + 14.f + text_w(t.unit_name.c_str()) + 10.f, 90.f, 240.f);
  }

  // ── block box geometry / drawing ─────────────────────────────────────────
  struct BoxGeo
  {
    float z;             // active zoom (screen px per world unit)
    float bx, by, bw, bh;
    float badge_x, badge_y, badge;
    float nx, ny, nw, nh;
    // Method-only: the "add argument" (+) circle, in screen coordinates.
    float plus_cx, plus_cy, plus_r;
  };

  // (World-unit) height of a method box for the given argument count: the
  // compact header, the + row, one row per argument, plus a bottom margin.
  static float method_height_for(int n_args) { return BOX_H + ROW_H * (1.f + static_cast<float>(n_args)) + PAD; }

  // (World-unit) height of a block: methods grow to hold their + row and args;
  // fields stay compact.
  static float box_height(const Block &s)
  {
    return s.type == BlockType::Method ? method_height_for(static_cast<int>(s.args.size())) : BOX_H;
  }

  // Layout is computed at scale 1 (world/unzoomed) and multiplied by the zoom,
  // so the box and everything inside it scale together.
  static BoxGeo box_geo(const EditorView &e, const Block &s)
  {
    const float z = static_cast<float>(e.cur() ? e.cur()->zoom : 1.0);
    BoxGeo      g{};
    g.z  = z;
    g.bx = e.to_screen_x(s.x);
    g.by = e.to_screen_y(s.y);
    g.bw = s.width * z;
    g.bh = box_height(s) * z;

    g.badge   = BADGE * z;
    g.badge_x = g.bx + PAD * z;
    g.badge_y = g.by + PAD * z; // top padding == PAD == left

    g.nx = g.bx + (PAD + BADGE + GAP) * z;
    g.nh = std::min(BOX_H * z - 6.f, FS + 8.f);
    g.ny = g.by + (BOX_H * z - g.nh) * .5f;
    g.nw = std::max(g.bx + g.bw - g.nx - GAP * z, 10.f);

    // The + circle sits in its own row below the argument rows, centred under
    // the badge.
    const int n_args = static_cast<int>(s.args.size());
    g.plus_cx        = g.bx + (PAD + BADGE * .5f) * z;
    g.plus_cy        = g.by + (BOX_H + ROW_H * static_cast<float>(n_args) + ROW_H * .5f) * z;
    g.plus_r         = PLUS_D * .5f * z;
    return g;
  }

  // Top (screen y) of argument row `i` for a method box (rows follow the header).
  static float arg_row_y(const BoxGeo &g, int i) { return g.by + (BOX_H + ROW_H * i) * g.z; }

  // Unscaled (world-unit) box width needed to fit `name`: matches box_geo's
  // horizontal layout (left pad + badge + gap + text + right pad).
  static float fit_width(const std::string &name)
  {
    const float w = PAD + BADGE + GAP + text_w(name.c_str()) + GAP;
    return std::max(w, 60.f);
  }

  // Width needed to fit a block's name and (for methods) every argument name.
  static float block_fit_width(const Block &s)
  {
    float w = fit_width(s.name);
    for (const MethodArg &a : s.args) w = std::max(w, fit_width(a.name));
    return w;
  }

  static void draw_box(SDL_Renderer *ren, const BoxGeo &g, char letter, const char *name, bool hovered, float scale)
  {
    fill(ren, hovered ? C_HOVER : C_PANEL, g.bx, g.by, g.bw, g.bh);
    rect(ren, C_BORDER, g.bx, g.by, g.bw, g.bh);

    fill(ren, C_ACCENT, g.badge_x, g.badge_y, g.badge, g.badge);
    const char ls[2] = {letter, '\0'};
    text_draw_scaled(ren, ls, g.badge_x + (g.badge - text_w(ls) * scale) * .5f, center_baseline_scaled(g.badge_y, g.badge, scale), C_PANEL, scale);

    // Centre the name in the header band (the badge plus its top/bottom pad),
    // not in the full box — methods extend below with the + row and args.
    const float header_h = 2.f * (g.badge_y - g.by) + g.badge;
    if (name && *name) text_draw_scaled(ren, name, g.nx, center_baseline_scaled(g.by, header_h, scale), C_TEXT, scale);
  }

  // Filled circle via horizontal scanlines (SDL has no circle primitive).
  static void fill_circle(SDL_Renderer *r, Clr c, float cx, float cy, float rad)
  {
    sc(r, c);
    const int r0 = static_cast<int>(std::ceil(rad));
    for (int dy = -r0; dy <= r0; dy++) {
      const float dx = std::sqrt(std::max(0.f, rad * rad - static_cast<float>(dy) * dy));
      SDL_FRect   line{cx - dx, cy + static_cast<float>(dy), 2.f * dx, 1.f};
      SDL_RenderFillRect(r, &line);
    }
  }

  // Circle outline (SDL has no circle primitive): a closed polyline. Works over
  // any background, unlike a two-fill ring.
  static void stroke_circle(SDL_Renderer *r, Clr c, float cx, float cy, float rad)
  {
    sc(r, c);
    const int   segs = std::max(16, static_cast<int>(rad * 2.f));
    float       px = cx + rad, py = cy;
    for (int i = 1; i <= segs; i++) {
      const float a = static_cast<float>(i) / static_cast<float>(segs) * 6.2831853f;
      const float x = cx + rad * std::cos(a), y = cy + rad * std::sin(a);
      SDL_RenderLine(r, px, py, x, y);
      px = x;
      py = y;
    }
  }

  // Darken a colour by factor `f` (keeps alpha). Used for hover states.
  static Clr scale_clr(Clr c, float f)
  {
    return Clr{static_cast<Uint8>(c.r * f), static_cast<Uint8>(c.g * f), static_cast<Uint8>(c.b * f), c.a};
  }

  // A small round +/- control inside `accent` circle. `plus` adds the vertical
  // bar. On hover the circle darkens slightly (a dimmed accent, not the
  // near-black panel hover).
  static void draw_round_btn(SDL_Renderer *ren, float cx, float cy, float r, bool plus, bool hovered, Clr accent)
  {
    fill_circle(ren, hovered ? scale_clr(accent, .8f) : accent, cx, cy, r);
    const float arm   = r * .55f;
    const float thick = std::max(1.5f, r * .28f);
    fill(ren, C_PANEL, cx - arm, cy - thick * .5f, 2.f * arm, thick);
    if (plus) fill(ren, C_PANEL, cx - thick * .5f, cy - arm, thick, 2.f * arm);
  }

  // Centre + radius (screen) of the delete (−) control for argument row `i`. It
  // sits in the same left column as the + control, beside the argument's name.
  static void arg_minus(const BoxGeo &g, int i, float &cx, float &cy, float &r)
  {
    cx = g.plus_cx;
    cy = arg_row_y(g, i) + ROW_H * g.z * .5f;
    r  = PLUS_D * .5f * g.z;
  }

  static bool hit_circle(float mx, float my, float cx, float cy, float r)
  {
    const float dx = mx - cx, dy = my - cy;
    return dx * dx + dy * dy <= r * r;
  }

  // Index of the argument row under the cursor for a method box, or -1.
  static int arg_row_at(const BoxGeo &g, const Block &s, float mx, float my)
  {
    if (s.type != BlockType::Method) return -1;
    for (int i = 0; i < static_cast<int>(s.args.size()); i++)
      if (hit(mx, my, g.bx, arg_row_y(g, i), g.bw, ROW_H * g.z)) return i;
    return -1;
  }

  // Index of the argument whose − (delete) control is under the cursor, or -1.
  static int arg_minus_at(const BoxGeo &g, const Block &s, float mx, float my)
  {
    if (s.type != BlockType::Method) return -1;
    for (int i = 0; i < static_cast<int>(s.args.size()); i++) {
      float cx, cy, r;
      arg_minus(g, i, cx, cy, r);
      if (hit_circle(mx, my, cx, cy, r)) return i;
    }
    return -1;
  }

  // Draw the type badge of a block: `letter` centred in a square (or a circle
  // when `circle`) filled with `accent`. When `crossed`, two diagonals of the
  // badge square are drawn over the letter (used to strike out a destructor).
  static void draw_badge(SDL_Renderer *ren, const BoxGeo &g, char letter, Clr accent, float z, bool circle, bool crossed)
  {
    if (circle)
      fill_circle(ren, accent, g.badge_x + g.badge * .5f, g.badge_y + g.badge * .5f, g.badge * .5f);
    else
      fill(ren, accent, g.badge_x, g.badge_y, g.badge, g.badge);

    const char ls[2] = {letter, '\0'};
    text_draw_scaled(ren, ls, g.badge_x + (g.badge - text_w(ls) * z) * .5f, center_baseline_scaled(g.badge_y, g.badge, z), C_PANEL, z);

    if (crossed) {
      sc(ren, C_PANEL);
      SDL_RenderLine(ren, g.badge_x, g.badge_y, g.badge_x + g.badge, g.badge_y + g.badge);
      SDL_RenderLine(ren, g.badge_x + g.badge, g.badge_y, g.badge_x, g.badge_y + g.badge);
    }
  }

  // Draw a canvas block: box, per-element hover highlight, badge, name and (for
  // methods) the argument rows (each with a − delete control) plus the + control.
  // `skip_arg_id`, when non-empty, is the argument being edited inline (drawn as
  // the input field instead). hov_name / hov_arg / hov_plus / hov_minus pick
  // which element is highlighted.
  static void draw_block(SDL_Renderer      *ren,
                         const BoxGeo      &g,
                         const Block       &s,
                         char               letter,
                         bool               blank_name,
                         const std::string &skip_arg_id,
                         bool               hov_name,
                         int                hov_arg,
                         bool               hov_plus,
                         int                hov_minus)
  {
    const float z = g.z;
    // A deactivated block renders dim and monochrome.
    const bool disabled = s.disabled;
    const Clr  accent   = disabled ? C_DIM : C_ACCENT;
    const Clr  txt      = disabled ? C_DIM : C_TEXT;

    fill(ren, C_PANEL, g.bx, g.by, g.bw, g.bh);
    rect(ren, C_BORDER, g.bx, g.by, g.bw, g.bh);

    // Per-element hover highlight: a full-width band behind the text.
    if (hov_name) fill(ren, C_HOVER, g.bx + 1.f, g.by + 1.f, g.bw - 2.f, BOX_H * z - 2.f);
    if (hov_arg >= 0) fill(ren, C_HOVER, g.bx + 1.f, arg_row_y(g, hov_arg), g.bw - 2.f, ROW_H * z);

    // Badge (drawn on top of any highlight). The method type tweaks its look:
    // constructor shows a K, an inner method sits in a circle (not a square),
    // a destructor's letter is crossed out diagonally.
    char badge_letter = letter;
    bool circle = false, crossed = false;
    if (s.type == BlockType::Method) {
      switch (s.method_type) {
        case MethodType::Constructor: badge_letter = 'K'; break;
        case MethodType::Inner: circle = true; break;
        case MethodType::Destructor: crossed = true; break;
        default: break; // Static: plain square M
      }
    }
    draw_badge(ren, g, badge_letter, accent, z, circle, crossed);

    // Access level as protective "walls" enclosing the badge (following its
    // shape): Public — none, Protected — one wall, Private — two walls. More
    // walls reads as more enclosed, hence more protected.
    const int walls = s.access == MethodAccess::Private ? 2 : (s.access == MethodAccess::Protected ? 1 : 0);
    const float cx = g.badge_x + g.badge * .5f, cy = g.badge_y + g.badge * .5f;
    for (int w = 0; w < walls; w++) {
      const float d = (2.f + 2.f * static_cast<float>(w)) * z; // gap of the w-th wall from the badge
      if (circle)
        stroke_circle(ren, accent, cx, cy, g.badge * .5f + d);
      else
        rect(ren, accent, g.badge_x - d, g.badge_y - d, g.badge + 2.f * d, g.badge + 2.f * d);
    }

    // Name, centred in the header band.
    const float header_h = 2.f * (g.badge_y - g.by) + g.badge;
    if (!blank_name && !s.name.empty())
      text_draw_scaled(ren, s.name.c_str(), g.nx, center_baseline_scaled(g.by, header_h, z), txt, z);

    if (s.type != BlockType::Method) return;

    for (int i = 0; i < static_cast<int>(s.args.size()); i++) {
      if (s.args[i].id == skip_arg_id) continue;
      text_draw_scaled(ren, s.args[i].name.c_str(), g.nx, center_baseline_scaled(arg_row_y(g, i), ROW_H * z, z), txt, z);
      float cx, cy, r;
      arg_minus(g, i, cx, cy, r);
      draw_round_btn(ren, cx, cy, r, false, hov_minus == i, accent);
    }
    draw_round_btn(ren, g.plus_cx, g.plus_cy, g.plus_r, true, hov_plus, accent);
  }

  // ── lifecycle ────────────────────────────────────────────────────────────────
  void EditorView::open_for(const Conn &c, const std::string &schema_, const std::string &uid, const std::string &uname, UnitType utype)
  {
    for (int i = 0; i < static_cast<int>(tabs.size()); i++) {
      if (tabs[i].unit_id == uid && tabs[i].schema == schema_ && tabs[i].conn.name == c.name) {
        active = i;
        open   = true;
        return;
      }
    }
    EditorTab t{};
    t.conn      = c;
    t.schema    = schema_;
    t.unit_id   = uid;
    t.unit_name = uname;
    t.unit_type = utype;
    tabs.push_back(std::move(t));
    active       = static_cast<int>(tabs.size()) - 1;
    open         = true;
    editing      = false;
    chooser_open = false;
    panning      = false;
    method_menu_open = false;
  }

  void EditorView::close()
  {
    open         = false;
    editing      = false;
    chooser_open = false;
    panning      = false;
    method_menu_open = false;
  }

  void EditorView::close_tab(int i)
  {
    if (i < 0 || i >= static_cast<int>(tabs.size())) return;
    tabs.erase(tabs.begin() + i);
    editing      = false;
    chooser_open = false;
    panning      = false;
    method_menu_open = false;
    if (tabs.empty()) {
      active = -1;
      open   = false;
      return;
    }
    if (active > i)
      active--;
    else if (active >= static_cast<int>(tabs.size()))
      active = static_cast<int>(tabs.size()) - 1;
  }

  int EditorView::tab_at(float mx, float my) const
  {
    if (my < py || my >= py + TAB_H) return -1;
    float x = px;
    for (int i = 0; i < static_cast<int>(tabs.size()); i++) {
      float w = tab_width(tabs[i]);
      if (mx >= x && mx < x + w) return i;
      x += w;
    }
    return -1;
  }

  void EditorView::init_camera()
  {
    EditorTab *t = cur();
    if (!t) return;

    if (auto saved = load_unit_editor_coord(t->unit_id)) {
      // Restore the persisted coordinate system for this unit.
      t->zoom  = std::clamp(saved->zoom, static_cast<double>(ZOOM_MIN), static_cast<double>(ZOOM_MAX));
      t->cam_x = saved->cam_x;
      t->cam_y = saved->cam_y;
    } else {
      t->zoom         = 1.0;
      auto [box, err] = block_bbox_for_unit(t->conn, t->schema, t->unit_id);
      if (box) {
        const double mx = (box->min_x + box->max_x) * .5;
        const double my = (box->min_y + box->max_y) * .5;
        t->cam_x        = mx - (cw * .5) / t->zoom;
        t->cam_y        = my - (ch * .5) / t->zoom;
      } else {
        t->cam_x = -(cw * .5) / t->zoom;
        t->cam_y = -(ch * .5) / t->zoom;
      }
    }
    t->cam_init = true;
    t->last_cw  = cw;
    t->last_ch  = ch;
    reload();
  }

  void EditorView::save_view_state()
  {
    EditorTab *t = cur();
    if (t) save_unit_editor_coord(t->unit_id, {t->zoom, t->cam_x, t->cam_y});
  }

  void EditorView::reload()
  {
    EditorTab *t = cur();
    if (!t) return;
    const float minx = static_cast<float>(t->cam_x);
    const float miny = static_cast<float>(t->cam_y);
    const float maxx = static_cast<float>(t->cam_x + cw / t->zoom);
    const float maxy = static_cast<float>(t->cam_y + ch / t->zoom);
    auto [rows, err] = load_blocks_in_view(t->conn, t->schema, t->unit_id, minx, miny, maxx, maxy);
    t->blocks         = std::move(rows);
  }

  void EditorView::start_edit_name(const Block &s, float fbx, float fby, float fbw, float fbh)
  {
    editing       = true;
    edit_is_arg   = false;
    edit_arg_id.clear();
    edit_id       = s.id;
    edit_type     = s.type;
    edit_field.ed = TextEditor{};
    edit_field.ed.set(s.name);
    edit_field.ctx.open = false;
    edit_bx             = fbx;
    edit_by             = fby;
    edit_bw             = fbw;
    edit_bh             = fbh;
  }

  void EditorView::start_edit_arg(const Block &m, const MethodArg &a, float fbx, float fby, float fbw, float fbh)
  {
    editing       = true;
    edit_is_arg   = true;
    edit_arg_id   = a.id;
    edit_id       = m.id; // owning method (used to resize the box on commit)
    edit_type     = m.type;
    edit_field.ed = TextEditor{};
    edit_field.ed.set(a.name);
    edit_field.ctx.open = false;
    edit_bx             = fbx;
    edit_by             = fby;
    edit_bw             = fbw;
    edit_bh             = fbh;
  }

  void EditorView::add_arg(const Block &m)
  {
    EditorTab *t = cur();
    if (!t) return;
    auto [id, err] = append_method_arg(t->conn, t->schema, m.id, "новыйАргумент");
    if (id.empty()) return;
    // Resize the box for the extra row and persist before reloading.
    const float w = std::max(block_fit_width(m), fit_width("новыйАргумент"));
    const float h = method_height_for(static_cast<int>(m.args.size()) + 1);
    update_block_size(t->conn, t->schema, m.id, w, h);
    reload();
  }

  void EditorView::del_arg(const Block &m, const std::string &arg_id)
  {
    EditorTab *t = cur();
    if (!t) return;
    auto [ok, err] = delete_method_arg(t->conn, t->schema, arg_id);
    if (!ok) return;
    // Recompute the box size from the remaining arguments and persist.
    float w         = fit_width(m.name);
    int   remaining = 0;
    for (const MethodArg &a : m.args) {
      if (a.id == arg_id) continue;
      w = std::max(w, fit_width(a.name));
      remaining++;
    }
    update_block_size(t->conn, t->schema, m.id, w, method_height_for(remaining));
    reload();
  }

  void EditorView::draw_method_menu(SDL_Renderer *ren, float mx, float my, bool ldown, bool rdown)
  {
    EditorTab *t = cur();
    if (!t) {
      method_menu_open = false;
      return;
    }
    const Block *m = nullptr;
    for (const Block &b : t->blocks)
      if (b.id == method_menu_id) {
        m = &b;
        break;
      }
    if (!m) {
      method_menu_open = false;
      return;
    }
    const bool is_method = m->type == BlockType::Method;

    // Build the menu: for methods the method-type group first, then (both kinds)
    // the access group and the deactivate/activate toggle at the bottom. A dot
    // marks the current value (shown normally, but not selectable); the other
    // values have no marker.
    enum { ACT_TOGGLE, ACT_DELETE, ACT_INNER, ACT_STATIC, ACT_CTOR, ACT_DTOR, ACT_PRIVATE, ACT_PROTECTED, ACT_PUBLIC };
    enum Mark { MARK_NONE, MARK_CURRENT, MARK_SWITCH };
    struct Item
    {
      std::string label;
      bool        sep;
      int         action; // -1 for separators
      Mark        mark;
    };
    auto grp = [](bool current, const char *name, int action) {
      return Item{name, false, action, current ? MARK_CURRENT : MARK_SWITCH};
    };

    std::vector<Item> items;
    if (is_method) {
      items.push_back(grp(m->method_type == MethodType::Inner, "Внутренний", ACT_INNER));
      items.push_back(grp(m->method_type == MethodType::Static, "Статичный", ACT_STATIC));
      items.push_back(grp(m->method_type == MethodType::Constructor, "Конструктор", ACT_CTOR));
      items.push_back(grp(m->method_type == MethodType::Destructor, "Деструктор", ACT_DTOR));
      items.push_back({"", true, -1, MARK_NONE});
    }
    items.push_back(grp(m->access == MethodAccess::Private, "Приватный", ACT_PRIVATE));
    items.push_back(grp(m->access == MethodAccess::Protected, "Защищённый", ACT_PROTECTED));
    items.push_back(grp(m->access == MethodAccess::Public, "Всеобщий", ACT_PUBLIC));
    items.push_back({"", true, -1, MARK_NONE});
    items.push_back({m->disabled ? "Активировать" : "Деактивировать", false, ACT_TOGGLE, MARK_NONE});
    items.push_back({"", true, -1, MARK_NONE});
    items.push_back({"Удалить", false, ACT_DELETE, MARK_NONE});

    // A marker gutter on the left keeps every label aligned, marked or not.
    constexpr float IH = 24.f, SEP_H = 7.f, PADX = 10.f, GUTTER = 16.f;
    const float     label_x = PADX + GUTTER;
    float           w       = 120.f;
    for (const Item &it : items)
      if (!it.sep) w = std::max(w, label_x + text_w(it.label.c_str()) + PADX);
    float h = 4.f;
    for (const Item &it : items) h += it.sep ? SEP_H : IH;

    const float ox = std::clamp(method_menu_x, cx, cx + cw - w);
    const float oy = std::clamp(method_menu_y, cy, cy + ch - h);

    // A click outside the menu dismisses it.
    if ((ldown || rdown) && !hit(mx, my, ox, oy, w, h)) {
      method_menu_open = false;
      return;
    }

    fill(ren, C_DLGBG, ox, oy, w, h);
    rect(ren, C_BORDER, ox, oy, w, h);

    int   chosen = -1;
    float iy     = oy + 2.f;
    for (const Item &it : items) {
      if (it.sep) {
        fill(ren, C_BORDER, ox + 6.f, iy + SEP_H * .5f, w - 12.f, 1.f);
        iy += SEP_H;
        continue;
      }
      const bool selectable = it.mark != MARK_CURRENT; // the current value can't be re-picked
      const bool hov        = selectable && hit(mx, my, ox, iy, w, IH);
      if (hov) fill(ren, C_HOVER, ox + 1.f, iy, w - 2.f, IH);

      if (it.mark == MARK_CURRENT)
        fill_circle(ren, C_ACCENT, ox + PADX + GUTTER * .5f, iy + IH * .5f, 3.f);

      text_draw(ren, it.label.c_str(), ox + label_x, center_baseline(iy, IH), C_TEXT);
      if (ldown && hov) chosen = it.action;
      iy += IH;
    }

    if (chosen < 0) return;

    // Access and the disabled toggle exist on both kinds; the method-type group
    // is method-only.
    auto set_access = [&](MethodAccess a) {
      if (is_method)
        update_method_access(t->conn, t->schema, m->id, a);
      else
        update_field_access(t->conn, t->schema, m->id, a);
    };
    switch (chosen) {
      case ACT_TOGGLE:
        if (is_method)
          update_method_disabled(t->conn, t->schema, m->id, !m->disabled);
        else
          update_field_disabled(t->conn, t->schema, m->id, !m->disabled);
        break;
      case ACT_DELETE: delete_block(t->conn, t->schema, m->id, m->type); break;
      case ACT_INNER: update_method_type(t->conn, t->schema, m->id, MethodType::Inner); break;
      case ACT_STATIC: update_method_type(t->conn, t->schema, m->id, MethodType::Static); break;
      case ACT_CTOR: update_method_type(t->conn, t->schema, m->id, MethodType::Constructor); break;
      case ACT_DTOR: update_method_type(t->conn, t->schema, m->id, MethodType::Destructor); break;
      case ACT_PRIVATE: set_access(MethodAccess::Private); break;
      case ACT_PROTECTED: set_access(MethodAccess::Protected); break;
      case ACT_PUBLIC: set_access(MethodAccess::Public); break;
    }
    method_menu_open = false;
    reload();
  }

  void EditorView::commit_edit()
  {
    if (!editing) return;
    EditorTab *t = cur();
    if (t) {
      const std::string name = edit_field.ed.buf;
      if (edit_is_arg)
        update_method_arg_name(t->conn, t->schema, edit_arg_id, name);
      else
        update_block_name(t->conn, t->schema, edit_id, edit_type, name);

      // Recompute and persist the owning block's size from in-memory state with
      // the edit applied (the name just changed may be the widest entry).
      for (Block &b : t->blocks) {
        if (b.id != edit_id) continue;
        if (edit_is_arg) {
          for (MethodArg &a : b.args)
            if (a.id == edit_arg_id) {
              a.name = name;
              break;
            }
        } else {
          b.name = name;
        }
        update_block_size(t->conn, t->schema, b.id, block_fit_width(b), box_height(b));
        break;
      }
    }
    editing     = false;
    edit_is_arg = false;
    reload();
  }

  // ── event hooks ──────────────────────────────────────────────────────────────
  void EditorView::on_wheel(float dy, float mx, float my)
  {
    if (!open || editing || chooser_open) return;
    EditorTab *t = cur();
    if (!t || !hit(mx, my, cx, cy, cw, ch)) return;

    const double wx     = to_world_x(mx);
    const double wy     = to_world_y(my);
    const double factor = dy > 0 ? 1.15 : 1.0 / 1.15;
    t->zoom             = std::clamp(t->zoom * factor, static_cast<double>(ZOOM_MIN), static_cast<double>(ZOOM_MAX));
    t->cam_x            = wx - (mx - cx) / t->zoom;
    t->cam_y            = wy - (my - cy) / t->zoom;
    reload();
    save_view_state();
  }

  void EditorView::on_mouse_move(float mx, float my)
  {
    if (!open) return;
    if (editing) edit_field.on_move(mx);
    EditorTab *t = cur();
    if (!t) return;

    if (dragging) {
      const float dx = mx - drag_last_x;
      const float dy = my - drag_last_y;
      if (dx != 0 || dy != 0) {
        for (Block &s : t->blocks)
          if (s.id == drag_id) {
            s.x += static_cast<float>(dx / t->zoom);
            s.y += static_cast<float>(dy / t->zoom);
            break;
          }
        drag_moved = true;
      }
      drag_last_x = mx;
      drag_last_y = my;
    }

    if (dragging_arg) {
      Block *owner = nullptr;
      for (Block &s : t->blocks)
        if (s.id == drag_arg_owner) {
          owner = &s;
          break;
        }
      if (owner) {
        const BoxGeo g   = box_geo(*this, *owner);
        const int    n   = static_cast<int>(owner->args.size());
        int          cur = -1;
        for (int i = 0; i < n; i++)
          if (owner->args[i].id == drag_arg_id) {
            cur = i;
            break;
          }
        if (cur >= 0) {
          // Row under the cursor (rows start below the header, ROW_H apart).
          int tgt = static_cast<int>(std::floor((my - (g.by + BOX_H * g.z)) / (ROW_H * g.z)));
          tgt     = std::clamp(tgt, 0, n - 1);
          if (tgt != cur) {
            const MethodArg a = owner->args[cur];
            owner->args.erase(owner->args.begin() + cur);
            owner->args.insert(owner->args.begin() + tgt, a);
            arg_drag_moved = true;
          }
        }
      }
    }

    if (panning) {
      const float dx = mx - pan_last_x;
      const float dy = my - pan_last_y;
      if (dx != 0 || dy != 0) {
        t->cam_x -= dx / t->zoom;
        t->cam_y -= dy / t->zoom;
        panned = true;
      }
      pan_last_x = mx;
      pan_last_y = my;
    }
  }

  void EditorView::on_mouse_up()
  {
    if (!open) return;
    if (editing) edit_field.on_release();
    if (dragging) {
      EditorTab *t = cur();
      if (t && drag_moved) {
        for (const Block &s : t->blocks)
          if (s.id == drag_id) {
            update_block_position(t->conn, t->schema, s.id, s.x, s.y);
            break;
          }
        reload();
      }
      dragging   = false;
      drag_moved = false;
    }
    if (dragging_arg) {
      EditorTab *t = cur();
      if (t && arg_drag_moved) {
        for (const Block &s : t->blocks)
          if (s.id == drag_arg_owner) {
            std::vector<std::string> ids;
            for (const MethodArg &a : s.args) ids.push_back(a.id);
            reorder_method_args(t->conn, t->schema, s.id, ids);
            break;
          }
        reload();
      }
      dragging_arg   = false;
      arg_drag_moved = false;
    }
  }

  void EditorView::on_middle_down(float mx, float my)
  {
    if (!open) return;
    int i = tab_at(mx, my);
    if (i >= 0) {
      close_tab(i);
      return;
    }
    // Middle-drag pans the canvas.
    if (hit(mx, my, cx, cy, cw, ch)) {
      panning    = true;
      panned     = false;
      pan_last_x = mx;
      pan_last_y = my;
    }
  }

  void EditorView::on_middle_up()
  {
    if (!open) return;
    if (panning) {
      panning = false;
      if (panned) {
        reload();
        save_view_state();
      }
      panned = false;
    }
  }

  bool EditorView::handle_key(SDL_Keycode key, SDL_Keymod mod)
  {
    if (!open || !editing) return false;
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
      commit_edit();
      return true;
    }
    if (key == SDLK_ESCAPE) {
      editing = false; // cancel without saving
      return true;
    }
    return edit_field.handle_key(key, mod);
  }

  void EditorView::handle_text(const char *t)
  {
    if (open && editing) edit_field.handle_text(t);
  }

  // ── chooser popup ────────────────────────────────────────────────────────────
  static void draw_chooser(EditorView &e, SDL_Renderer *ren, float mx, float my, bool ldown)
  {
    EditorTab *t = e.cur();
    if (!t) return;

    constexpr float PAD = 8.f, SW = 180.f, SH = 40.f, GAP = 8.f;
    const float     panel_w = SW + 2 * PAD;
    const float     panel_h = 2 * SH + GAP + 2 * PAD;

    float ox = std::clamp(e.chooser_sx, e.cx, e.cx + e.cw - panel_w);
    float oy = std::clamp(e.chooser_sy, e.cy, e.cy + e.ch - panel_h);

    fill(ren, C_DLGBG, ox, oy, panel_w, panel_h);
    rect(ren, C_BORDER, ox, oy, panel_w, panel_h);

    auto sample = [&](float sy, char letter, const char *name) -> bool {
      BoxGeo g{};
      g.bx      = ox + PAD;
      g.by      = sy;
      g.bw      = SW;
      g.bh      = SH;
      g.badge   = BADGE;
      g.badge_x = g.bx + PAD;
      g.badge_y = g.by + (g.bh - g.badge) * .5f;
      g.nx      = g.badge_x + g.badge + GAP;
      bool hov  = hit(mx, my, g.bx, g.by, g.bw, g.bh);
      draw_box(ren, g, letter, name, hov, 1.f);
      return hov;
    };

    bool hm = sample(oy + PAD, 'M', "newMethod");
    bool hf = sample(oy + PAD + SH + GAP, 'F', "newField");

    if (ldown) {
      if (hm) {
        create_block(t->conn,
                         t->schema,
                         t->unit_id,
                         BlockType::Method,
                         e.chooser_wx,
                         e.chooser_wy,
                         fit_width("newMethod"),
                         method_height_for(0),
                         "newMethod");
        e.chooser_open = false;
        e.reload();
      } else if (hf) {
        create_block(t->conn, t->schema, t->unit_id, BlockType::Field, e.chooser_wx, e.chooser_wy, fit_width("newField"), BOX_H, "newField");
        e.chooser_open = false;
        e.reload();
      } else {
        e.chooser_open = false;
      }
    }
  }

  // ── tab strip ────────────────────────────────────────────────────────────────
  void EditorView::render(
      SDL_Renderer *ren, float pane_x, float pane_y, float pane_w, float pane_h, float mx, float my, bool ldown, bool rdown, int clicks)
  {
    if (!open) return;
    if (active < 0 || tabs.empty()) {
      open = false;
      return;
    }
    px = pane_x;
    py = pane_y;
    pw = pane_w;
    ph = pane_h;
    cx = px;
    cy = py + TAB_H;
    cw = pw;
    ch = ph - TAB_H;

    const bool dbl         = ldown && clicks >= 2;
    const int  hovered_tab = tab_at(mx, my);

    // Tab strip.
    fill(ren, C_BG, px, py, pw, TAB_H);
    float tx = px;
    for (int i = 0; i < static_cast<int>(tabs.size()); i++) {
      const float tw  = tab_width(tabs[i]);
      const bool  act = i == active;
      fill(ren, act ? C_PANEL : (hovered_tab == i ? C_HOVER : C_BG), tx, py, tw, TAB_H);
      fill(ren, C_BORDER, tx + tw - 1, py, 1, TAB_H);
      draw_unit_icon(ren, tx + 10, py + TAB_H * .5f, tabs[i].unit_type, act ? C_ACCENT : C_DIM);
      text_draw(ren, tabs[i].unit_name.c_str(), tx + 24, center_baseline(py, TAB_H), act ? C_TEXT : C_DIM);
      if (act) fill(ren, C_ACCENT, tx, py + TAB_H - 2, tw, 2);
      tx += tw;
    }
    fill(ren, C_BORDER, px, py + TAB_H - 1, pw, 1);

    // Left-click selects a tab.
    bool tab_clicked = false;
    if (ldown && hovered_tab >= 0) {
      if (editing) commit_edit();
      if (hovered_tab != active) {
        active           = hovered_tab;
        chooser_open     = false;
        method_menu_open = false;
        panning          = false;
      }
      tab_clicked = true;
    }

    EditorTab *t = cur();
    if (!t) return;

    if (!t->cam_init)
      init_camera();
    else if (cw != t->last_cw || ch != t->last_ch) {
      t->last_cw = cw;
      t->last_ch = ch;
      reload();
    }

    // Canvas.
    SDL_Rect clip{static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(cw), static_cast<int>(ch)};
    SDL_SetRenderClipRect(ren, &clip);

    // Grid.
    const float step = static_cast<float>(GRID * t->zoom);
    if (step >= 6.f) {
      sc(ren, C_PANEL);
      double first_x = std::floor(t->cam_x / GRID) * GRID;
      for (double wx = first_x; to_screen_x(wx) < cx + cw; wx += GRID) {
        float sx = to_screen_x(wx);
        if (sx >= cx) SDL_RenderLine(ren, sx, cy, sx, cy + ch);
      }
      double first_y = std::floor(t->cam_y / GRID) * GRID;
      for (double wy = first_y; to_screen_y(wy) < cy + ch; wy += GRID) {
        float sy = to_screen_y(wy);
        if (sy >= cy) SDL_RenderLine(ren, cx, sy, cx + cw, sy);
      }
    }

    // Block boxes.
    for (const Block &s : t->blocks) {
      BoxGeo      g            = box_geo(*this, s);
      const float z            = static_cast<float>(t->zoom);
      const bool  hov          = !editing && !chooser_open && !tab_clicked && hit(mx, my, g.bx, g.by, g.bw, g.bh);
      const char  letter       = s.type == BlockType::Field ? 'F' : 'M';
      const bool  editing_this = editing && s.id == edit_id;
      const bool  blank_name   = editing_this && !edit_is_arg;

      // Resolve which sub-element the cursor is over for a precise highlight.
      bool hov_name = false, hov_plus = false;
      int  hov_arg = -1, hov_minus = -1;
      if (hov) {
        if (s.type == BlockType::Method) {
          if (hit_circle(mx, my, g.plus_cx, g.plus_cy, g.plus_r))
            hov_plus = true;
          else if (int mi = arg_minus_at(g, s, mx, my); mi >= 0)
            hov_minus = mi;
          else if (int ai = arg_row_at(g, s, mx, my); ai >= 0)
            hov_arg = ai;
          else if (hit(mx, my, g.bx, g.by, g.bw, BOX_H * z))
            hov_name = true;
        } else {
          hov_name = true; // a field is a single name row
        }
      }

      draw_block(
          ren, g, s, letter, blank_name, editing_this && edit_is_arg ? edit_arg_id : std::string{}, hov_name, hov_arg, hov_plus, hov_minus);

      if (editing_this) {
        edit_bx = g.nx;
        edit_bw = g.nw;
        edit_bh = std::min(ROW_H * static_cast<float>(t->zoom) - 2.f, FS + 8.f);
        if (edit_is_arg) {
          int ai = 0;
          for (int i = 0; i < static_cast<int>(s.args.size()); i++)
            if (s.args[i].id == edit_arg_id) {
              ai = i;
              break;
            }
          const float ay = arg_row_y(g, ai);
          edit_by        = ay + (ROW_H * static_cast<float>(t->zoom) - edit_bh) * .5f;
        } else {
          edit_bh = std::min(BOX_H * static_cast<float>(t->zoom) - 6.f, FS + 8.f);
          edit_by = g.by + (BOX_H * static_cast<float>(t->zoom) - edit_bh) * .5f;
        }
        edit_field.draw(ren, edit_bx, edit_by, edit_bw, edit_bh, true);
      }
    }

    // Inline editing consumes all canvas interaction.
    if (editing) {
      const float text_ox = edit_bx + 6.f;
      if (ldown && !tab_clicked) {
        if (hit(mx, my, edit_bx, edit_by, edit_bw, edit_bh))
          edit_field.on_ldown(text_ox, mx, my, edit_bx, edit_by, edit_bw, edit_bh, clicks);
        else
          commit_edit();
      }
      SDL_SetRenderClipRect(ren, nullptr);
      return;
    }

    // Block context menu: open on right-click over a block's name header
    // (methods and fields both have one).
    if (!method_menu_open && rdown && !chooser_open && !tab_clicked && hit(mx, my, cx, cy, cw, ch)) {
      const Block *target = nullptr;
      for (const Block &s : t->blocks) {
        BoxGeo g = box_geo(*this, s);
        if (hit(mx, my, g.bx, g.by, g.bw, BOX_H * static_cast<float>(t->zoom))) target = &s;
      }
      if (target) {
        method_menu_open = true;
        method_menu_id   = target->id;
        method_menu_x    = mx;
        method_menu_y    = my;
      }
    }

    if (method_menu_open) {
      draw_method_menu(ren, mx, my, ldown && !tab_clicked, rdown);
      SDL_SetRenderClipRect(ren, nullptr);
      return;
    }

    if (chooser_open) {
      draw_chooser(*this, ren, mx, my, ldown && !tab_clicked);
      SDL_SetRenderClipRect(ren, nullptr);
      return;
    }

    if (tab_clicked) {
      SDL_SetRenderClipRect(ren, nullptr);
      return;
    }

    // Double-click: edit an argument, edit a box's name, or open the chooser on
    // empty canvas.
    if (dbl && hit(mx, my, cx, cy, cw, ch)) {
      const Block *target = nullptr;
      BoxGeo       tgeo{};
      for (const Block &s : t->blocks) {
        BoxGeo g = box_geo(*this, s);
        if (hit(mx, my, g.bx, g.by, g.bw, g.bh)) {
          target = &s;
          tgeo   = g;
        }
      }
      if (target) {
        const int ai = arg_row_at(tgeo, *target, mx, my);
        if (ai >= 0) {
          const float ay  = arg_row_y(tgeo, ai);
          const float fbh = std::min(ROW_H * tgeo.z - 2.f, FS + 8.f);
          const float fby = ay + (ROW_H * tgeo.z - fbh) * .5f;
          start_edit_arg(*target, target->args[ai], tgeo.nx, fby, tgeo.nw, fbh);
        } else if (hit(mx, my, tgeo.bx, tgeo.by, tgeo.bw, BOX_H * tgeo.z)) {
          const float fbh = std::min(BOX_H * tgeo.z - 6.f, FS + 8.f);
          const float fby = tgeo.by + (BOX_H * tgeo.z - fbh) * .5f;
          start_edit_name(*target, tgeo.nx, fby, tgeo.nw, fbh);
        }
        // A double-click on the + row falls through without editing.
      } else {
        chooser_open = true;
        chooser_wx   = static_cast<float>(to_world_x(mx));
        chooser_wy   = static_cast<float>(to_world_y(my));
        chooser_sx   = mx;
        chooser_sy   = my;
      }
      SDL_SetRenderClipRect(ren, nullptr);
      return;
    }

    // Single click on a method's + adds an argument; otherwise a click on a
    // block begins dragging it (the canvas is panned with the middle button).
    if (ldown && hit(mx, my, cx, cy, cw, ch)) {
      const Block *target = nullptr;
      BoxGeo       tgeo{};
      for (const Block &s : t->blocks) {
        BoxGeo g = box_geo(*this, s);
        if (hit(mx, my, g.bx, g.by, g.bw, g.bh)) {
          target = &s; // last hit wins (topmost)
          tgeo   = g;
        }
      }
      if (target) {
        int minus_i = arg_minus_at(tgeo, *target, mx, my);
        int arg_i   = target->type == BlockType::Method ? arg_row_at(tgeo, *target, mx, my) : -1;
        if (minus_i >= 0) {
          del_arg(*target, target->args[minus_i].id);
        } else if (target->type == BlockType::Method && hit_circle(mx, my, tgeo.plus_cx, tgeo.plus_cy, tgeo.plus_r)) {
          add_arg(*target);
        } else if (arg_i >= 0) {
          // Grab an argument row to reorder it within the method.
          dragging_arg   = true;
          arg_drag_moved = false;
          drag_arg_owner = target->id;
          drag_arg_id    = target->args[arg_i].id;
        } else {
          dragging    = true;
          drag_moved  = false;
          drag_id     = target->id;
          drag_last_x = mx;
          drag_last_y = my;
        }
      }
    }

    SDL_SetRenderClipRect(ren, nullptr);
  }

} // namespace front

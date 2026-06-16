#pragma once
#include "Clr.h"
#include "GlyphInfo.h"
#include "stb_truetype.h"
#include <SDL3/SDL.h>
#include <unicode/utypes.h>
#include <unordered_map>
#include <vector>

namespace front {

  constexpr float FS = 14.0f;

  struct FontAtlas
  {
    static constexpr int W = 2048, H = 2048, PAD = 1;

    SDL_Renderer                          *ren  = nullptr;
    SDL_Texture                           *tex  = nullptr;
    const SDL_PixelFormatDetails          *pfmt = nullptr;
    std::unordered_map<UChar32, GlyphInfo> cache;

    stbtt_fontinfo info;
    float          scale = 1.f;
    float          asc   = 0.f;
    float          desc  = 0.f;

    int cx = PAD, cy = PAD, rh = 0;

    void             init(SDL_Renderer *r, const unsigned char *ttf, float size);
    const GlyphInfo &get(UChar32 cp);

  private:
    const GlyphInfo &bake(UChar32 cp);
  };

  extern FontAtlas g_atlas;

  void  font_init(SDL_Renderer *ren);
  float text_draw(SDL_Renderer *ren, const char *s, float x, float y, Clr c);
  float text_draw_n(SDL_Renderer *ren, const char *s, int32_t byte_len, float x, float y, Clr c);
  // Like text_draw, but every glyph (size, bearing and advance) is multiplied by
  // `scale`. scale == 1 is identical to text_draw.
  float text_draw_scaled(SDL_Renderer *ren, const char *s, float x, float y, Clr c, float scale);
  float text_w_n(const char *s, int32_t byte_len);
  float text_w(const char *s);
  float center_baseline(float box_y, float box_h);
  // Baseline for vertically centring text scaled by `scale` in a box.
  float center_baseline_scaled(float box_y, float box_h, float scale);

} // namespace front

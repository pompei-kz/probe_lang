#define STB_TRUETYPE_IMPLEMENTATION
#include "FontAtlas.h"

#include <algorithm>
#include <cstring>
#include <unicode/utf8.h>
#include <vector>

#include "resources.hpp"

FontAtlas g_atlas;

// ── FontAtlas ─────────────────────────────────────────────────────────────────
void FontAtlas::init(SDL_Renderer *r, const unsigned char *ttf, float size)
{
  ren = r;
  stbtt_InitFont(&info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0));
  scale = stbtt_ScaleForPixelHeight(&info, size);
  int ai, di, lg;
  stbtt_GetFontVMetrics(&info, &ai, &di, &lg);
  asc  = ai * scale;
  desc = di * scale;

  pfmt = SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_RGBA32);
  tex  = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, W, H);
  SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

  std::vector<Uint32> blank(W * H, 0);
  SDL_UpdateTexture(tex, nullptr, blank.data(), W * sizeof(Uint32));
}

const GlyphInfo &FontAtlas::get(UChar32 cp)
{
  auto it = cache.find(cp);
  if (it != cache.end()) return it->second;
  return bake(cp);
}

const GlyphInfo &FontAtlas::bake(UChar32 cp)
{
  GlyphInfo      g{};
  int            gw, gh, gbx, gby;
  unsigned char *bmp = stbtt_GetCodepointBitmap(&info, 0, scale, static_cast<int>(cp), &gw, &gh, &gbx, &gby);

  if (bmp && gw > 0 && gh > 0) {
    if (cx + gw + PAD > W) {
      cx = PAD;
      cy += rh + PAD;
      rh = 0;
    }
    if (cy + gh + PAD <= H) {
      std::vector<Uint32> rgba(gw * gh);
      for (int i = 0; i < gw * gh; i++)
        rgba[i] = SDL_MapRGBA(pfmt, nullptr, 255, 255, 255, bmp[i]);
      SDL_Rect r{cx, cy, gw, gh};
      SDL_UpdateTexture(tex, &r, rgba.data(), gw * sizeof(Uint32));

      g.tx      = cx;
      g.ty      = cy;
      g.tw      = gw;
      g.th      = gh;
      g.bx      = gbx;
      g.by      = gby;
      g.visible = true;
      cx += gw + PAD;
      rh = std::max(rh, gh);
    }
  }

  int adv, lsb;
  stbtt_GetCodepointHMetrics(&info, static_cast<int>(cp), &adv, &lsb);
  g.adv = adv * scale;

  if (bmp) stbtt_FreeBitmap(bmp, nullptr);
  return cache.emplace(cp, g).first->second;
}

// ── free functions ────────────────────────────────────────────────────────────
void font_init(SDL_Renderer *ren)
{
  const auto *ttf = reinterpret_cast<const unsigned char *>(resources::fonts::Roboto_Regular_ttf.data());
  g_atlas.init(ren, ttf, FS);
}

float text_draw(SDL_Renderer *ren, const char *s, float x, float y, Clr c)
{
  SDL_SetTextureColorMod(g_atlas.tex, c.r, c.g, c.b);
  SDL_SetTextureAlphaMod(g_atlas.tex, c.a);
  float   px = x;
  int32_t i = 0, len = static_cast<int32_t>(strlen(s));
  while (i < len) {
    UChar32 cp;
    U8_NEXT(reinterpret_cast<const uint8_t *>(s), i, len, cp);
    if (cp < 0) continue;
    const auto &g = g_atlas.get(cp);
    if (g.visible) {
      SDL_FRect src{(float)g.tx, (float)g.ty, (float)g.tw, (float)g.th};
      SDL_FRect dst{px + g.bx, y + g.by, (float)g.tw, (float)g.th};
      SDL_RenderTexture(ren, g_atlas.tex, &src, &dst);
    }
    px += g.adv;
  }
  return px - x;
}

float text_draw_n(SDL_Renderer *ren, const char *s, int32_t byte_len, float x, float y, Clr c)
{
  SDL_SetTextureColorMod(g_atlas.tex, c.r, c.g, c.b);
  SDL_SetTextureAlphaMod(g_atlas.tex, c.a);
  float   px = x;
  int32_t i  = 0;
  while (i < byte_len) {
    UChar32 cp;
    U8_NEXT(reinterpret_cast<const uint8_t *>(s), i, byte_len, cp);
    if (cp < 0) continue;
    const auto &g = g_atlas.get(cp);
    if (g.visible) {
      SDL_FRect src{(float)g.tx, (float)g.ty, (float)g.tw, (float)g.th};
      SDL_FRect dst{px + g.bx, y + g.by, (float)g.tw, (float)g.th};
      SDL_RenderTexture(ren, g_atlas.tex, &src, &dst);
    }
    px += g.adv;
  }
  return px - x;
}

float text_w_n(const char *s, int32_t byte_len)
{
  float   px = 0;
  int32_t i  = 0;
  while (i < byte_len) {
    UChar32 cp;
    U8_NEXT(reinterpret_cast<const uint8_t *>(s), i, byte_len, cp);
    if (cp < 0) continue;
    px += g_atlas.get(cp).adv;
  }
  return px;
}

float text_w(const char *s)
{
  return text_w_n(s, static_cast<int32_t>(strlen(s)));
}

float center_baseline(float box_y, float box_h)
{
  return box_y + (box_h + g_atlas.asc + g_atlas.desc) * 0.5f;
}

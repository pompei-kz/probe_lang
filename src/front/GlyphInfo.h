#pragma once

namespace front {

  struct GlyphInfo
  {
    int   tx, ty, tw, th;
    int   bx, by;
    float adv;
    bool  visible;
  };

} // namespace front

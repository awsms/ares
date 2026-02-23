namespace {

// The toast OSD uses a tiny built-in 5x7 bitmap font so it can rasterize into
// a compact ARGB overlay buffer without depending on the platform UI toolkit.
static auto upperAscii(char c) -> char {
  if(c >= 'a' && c <= 'z') return c - ('a' - 'A');
  return c;
}

static auto glyph5x7(char c) -> const u8* {
  static const u8 space[7] = {0,0,0,0,0,0,0};
  static const u8 unknown[7] = {0x0e,0x11,0x02,0x04,0x04,0x00,0x04};

  static const u8 A[7] = {0x0e,0x11,0x11,0x1f,0x11,0x11,0x11};
  static const u8 B[7] = {0x1e,0x11,0x11,0x1e,0x11,0x11,0x1e};
  static const u8 C[7] = {0x0e,0x11,0x10,0x10,0x10,0x11,0x0e};
  static const u8 D[7] = {0x1e,0x11,0x11,0x11,0x11,0x11,0x1e};
  static const u8 E[7] = {0x1f,0x10,0x10,0x1e,0x10,0x10,0x1f};
  static const u8 F[7] = {0x1f,0x10,0x10,0x1e,0x10,0x10,0x10};
  static const u8 G[7] = {0x0e,0x11,0x10,0x17,0x11,0x11,0x0e};
  static const u8 H[7] = {0x11,0x11,0x11,0x1f,0x11,0x11,0x11};
  static const u8 I[7] = {0x0e,0x04,0x04,0x04,0x04,0x04,0x0e};
  static const u8 J[7] = {0x01,0x01,0x01,0x01,0x01,0x11,0x0e};
  static const u8 K[7] = {0x11,0x12,0x14,0x18,0x14,0x12,0x11};
  static const u8 L[7] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1f};
  static const u8 M[7] = {0x11,0x1b,0x15,0x15,0x11,0x11,0x11};
  static const u8 N[7] = {0x11,0x19,0x15,0x13,0x11,0x11,0x11};
  static const u8 O[7] = {0x0e,0x11,0x11,0x11,0x11,0x11,0x0e};
  static const u8 P[7] = {0x1e,0x11,0x11,0x1e,0x10,0x10,0x10};
  static const u8 Q[7] = {0x0e,0x11,0x11,0x11,0x15,0x12,0x0d};
  static const u8 R[7] = {0x1e,0x11,0x11,0x1e,0x14,0x12,0x11};
  static const u8 S[7] = {0x0f,0x10,0x10,0x0e,0x01,0x01,0x1e};
  static const u8 T[7] = {0x1f,0x04,0x04,0x04,0x04,0x04,0x04};
  static const u8 U[7] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0e};
  static const u8 V[7] = {0x11,0x11,0x11,0x11,0x11,0x0a,0x04};
  static const u8 W[7] = {0x11,0x11,0x11,0x15,0x15,0x15,0x0a};
  static const u8 X[7] = {0x11,0x11,0x0a,0x04,0x0a,0x11,0x11};
  static const u8 Y[7] = {0x11,0x11,0x0a,0x04,0x04,0x04,0x04};
  static const u8 Z[7] = {0x1f,0x01,0x02,0x04,0x08,0x10,0x1f};

  static const u8 n0[7] = {0x0e,0x11,0x13,0x15,0x19,0x11,0x0e};
  static const u8 n1[7] = {0x04,0x0c,0x04,0x04,0x04,0x04,0x0e};
  static const u8 n2[7] = {0x0e,0x11,0x01,0x02,0x04,0x08,0x1f};
  static const u8 n3[7] = {0x1e,0x01,0x01,0x0e,0x01,0x01,0x1e};
  static const u8 n4[7] = {0x02,0x06,0x0a,0x12,0x1f,0x02,0x02};
  static const u8 n5[7] = {0x1f,0x10,0x10,0x1e,0x01,0x01,0x1e};
  static const u8 n6[7] = {0x0e,0x10,0x10,0x1e,0x11,0x11,0x0e};
  static const u8 n7[7] = {0x1f,0x01,0x02,0x04,0x08,0x08,0x08};
  static const u8 n8[7] = {0x0e,0x11,0x11,0x0e,0x11,0x11,0x0e};
  static const u8 n9[7] = {0x0e,0x11,0x11,0x0f,0x01,0x01,0x0e};

  static const u8 colon[7] = {0x00,0x04,0x00,0x00,0x04,0x00,0x00};
  static const u8 semicolon[7] = {0x00,0x04,0x00,0x00,0x04,0x04,0x08};
  static const u8 excl[7] = {0x04,0x04,0x04,0x04,0x04,0x00,0x04};
  static const u8 question[7] = {0x0e,0x11,0x01,0x02,0x04,0x00,0x04};
  static const u8 dash[7] = {0x00,0x00,0x00,0x1f,0x00,0x00,0x00};
  static const u8 plus[7] = {0x00,0x04,0x04,0x1f,0x04,0x04,0x00};
  static const u8 equals[7] = {0x00,0x00,0x1f,0x00,0x1f,0x00,0x00};
  static const u8 underscore[7] = {0x00,0x00,0x00,0x00,0x00,0x00,0x1f};
  static const u8 dot[7] = {0x00,0x00,0x00,0x00,0x00,0x00,0x04};
  static const u8 comma[7] = {0x00,0x00,0x00,0x00,0x04,0x04,0x08};
  static const u8 slash[7] = {0x01,0x02,0x02,0x04,0x08,0x08,0x10};
  static const u8 backslash[7] = {0x10,0x08,0x08,0x04,0x02,0x02,0x01};
  static const u8 apostrophe[7] = {0x04,0x04,0x08,0x00,0x00,0x00,0x00};
  static const u8 dquote[7] = {0x0a,0x0a,0x00,0x00,0x00,0x00,0x00};
  static const u8 lparen[7] = {0x02,0x04,0x08,0x08,0x08,0x04,0x02};
  static const u8 rparen[7] = {0x08,0x04,0x02,0x02,0x02,0x04,0x08};
  static const u8 lbracket[7] = {0x0e,0x08,0x08,0x08,0x08,0x08,0x0e};
  static const u8 rbracket[7] = {0x0e,0x02,0x02,0x02,0x02,0x02,0x0e};
  static const u8 lbrace[7] = {0x02,0x04,0x04,0x08,0x04,0x04,0x02};
  static const u8 rbrace[7] = {0x08,0x04,0x04,0x02,0x04,0x04,0x08};
  static const u8 lt[7] = {0x02,0x04,0x08,0x10,0x08,0x04,0x02};
  static const u8 gt[7] = {0x08,0x04,0x02,0x01,0x02,0x04,0x08};
  static const u8 star[7] = {0x00,0x15,0x0e,0x1f,0x0e,0x15,0x00};
  static const u8 amp[7] = {0x0c,0x12,0x14,0x08,0x15,0x12,0x0d};
  static const u8 percent[7] = {0x19,0x19,0x02,0x04,0x08,0x13,0x13};
  static const u8 hash[7] = {0x0a,0x0a,0x1f,0x0a,0x1f,0x0a,0x0a};
  static const u8 at[7] = {0x0e,0x11,0x17,0x15,0x17,0x10,0x0f};
  static const u8 dollar[7] = {0x04,0x0f,0x14,0x0e,0x05,0x1e,0x04};
  static const u8 caret[7] = {0x04,0x0a,0x11,0x00,0x00,0x00,0x00};
  static const u8 tilde[7] = {0x00,0x00,0x09,0x16,0x00,0x00,0x00};
  static const u8 backtick[7] = {0x08,0x04,0x02,0x00,0x00,0x00,0x00};

  switch(upperAscii(c)) {
  case 'A': return A; case 'B': return B; case 'C': return C; case 'D': return D; case 'E': return E;
  case 'F': return F; case 'G': return G; case 'H': return H; case 'I': return I; case 'J': return J;
  case 'K': return K; case 'L': return L; case 'M': return M; case 'N': return N; case 'O': return O;
  case 'P': return P; case 'Q': return Q; case 'R': return R; case 'S': return S; case 'T': return T;
  case 'U': return U; case 'V': return V; case 'W': return W; case 'X': return X; case 'Y': return Y;
  case 'Z': return Z;
  case '0': return n0; case '1': return n1; case '2': return n2; case '3': return n3; case '4': return n4;
  case '5': return n5; case '6': return n6; case '7': return n7; case '8': return n8; case '9': return n9;
  case ' ': return space;
  case ':': return colon;
  case ';': return semicolon;
  case '!': return excl;
  case '?': return question;
  case '-': return dash;
  case '+': return plus;
  case '=': return equals;
  case '_': return underscore;
  case '.': return dot;
  case ',': return comma;
  case '/': return slash;
  case '\\': return backslash;
  case '\'': return apostrophe;
  case '"': return dquote;
  case '(': return lparen;
  case ')': return rparen;
  case '[': return lbracket;
  case ']': return rbracket;
  case '{': return lbrace;
  case '}': return rbrace;
  case '<': return lt;
  case '>': return gt;
  case '*': return star;
  case '&': return amp;
  case '%': return percent;
  case '#': return hash;
  case '@': return at;
  case '$': return dollar;
  case '^': return caret;
  case '~': return tilde;
  case '`': return backtick;
  default: return unknown;
  }
}

// Blend a source ARGB pixel onto a transparent overlay buffer, modulating it by
// an additional global alpha used for toast fade-in/fade-out.
static auto blendPixelArgb(u32& dst, u32 src, u32 globalAlpha) -> void {
  u32 sa = (src >> 24) & 0xff;
  u32 sr = (src >> 16) & 0xff;
  u32 sg = (src >> 8) & 0xff;
  u32 sb = (src >> 0) & 0xff;
  u32 alpha = (sa * globalAlpha) / 255;
  if(alpha == 0) return;
  u32 dr = (dst >> 16) & 0xff;
  u32 dg = (dst >> 8) & 0xff;
  u32 db = (dst >> 0) & 0xff;
  u32 da = (dst >> 24) & 0xff;

  u32 outA = alpha + (da * (255 - alpha)) / 255;
  u32 outR = (sr * alpha + dr * da * (255 - alpha) / 255) / max<u32>(1, outA);
  u32 outG = (sg * alpha + dg * da * (255 - alpha) / 255) / max<u32>(1, outA);
  u32 outB = (sb * alpha + db * da * (255 - alpha) / 255) / max<u32>(1, outA);
  dst = (outA << 24) | (outR << 16) | (outG << 8) | outB;
}

// Icons are usually small cached badge/game images. Bilinear sampling keeps
// them from looking too jagged when the toast scales them up.
static auto sampleIconBilinear(const std::vector<u32>& icon, u32 iconW, u32 iconH, f64 fx, f64 fy) -> u32 {
  if(icon.empty() || iconW == 0 || iconH == 0) return 0;
  if(fx < 0.0) fx = 0.0;
  if(fy < 0.0) fy = 0.0;
  if(fx > (f64)(iconW - 1)) fx = (f64)(iconW - 1);
  if(fy > (f64)(iconH - 1)) fy = (f64)(iconH - 1);
  u32 x0 = (u32)fx;
  u32 y0 = (u32)fy;
  u32 x1 = min(iconW - 1, x0 + 1);
  u32 y1 = min(iconH - 1, y0 + 1);
  f64 tx = fx - (f64)x0;
  f64 ty = fy - (f64)y0;
  auto c00 = icon[y0 * iconW + x0];
  auto c10 = icon[y0 * iconW + x1];
  auto c01 = icon[y1 * iconW + x0];
  auto c11 = icon[y1 * iconW + x1];
  auto lerp = [](f64 a, f64 b, f64 t) -> f64 { return a + (b - a) * t; };
  auto chan = [&](u32 c, u32 shift) -> f64 { return (f64)((c >> shift) & 0xff); };
  f64 a0 = lerp(chan(c00, 24), chan(c10, 24), tx);
  f64 r0 = lerp(chan(c00, 16), chan(c10, 16), tx);
  f64 g0 = lerp(chan(c00, 8),  chan(c10, 8),  tx);
  f64 b0 = lerp(chan(c00, 0),  chan(c10, 0),  tx);
  f64 a1 = lerp(chan(c01, 24), chan(c11, 24), tx);
  f64 r1 = lerp(chan(c01, 16), chan(c11, 16), tx);
  f64 g1 = lerp(chan(c01, 8),  chan(c11, 8),  tx);
  f64 b1 = lerp(chan(c01, 0),  chan(c11, 0),  tx);
  u32 a = (u32)lerp(a0, a1, ty);
  u32 r = (u32)lerp(r0, r1, ty);
  u32 g = (u32)lerp(g0, g1, ty);
  u32 b = (u32)lerp(b0, b1, ty);
  return (a << 24) | (r << 16) | (g << 8) | b;
}

// Primitive rectangle fill used for the toast card background, accent bar, and
// glyph rasterization.
static auto fillRectBlend(u32* output, u32 pitch, u32 width, u32 height, s32 x, s32 y, s32 w, s32 h, u32 rgb, u32 alpha) -> void {
  s32 x0 = x, y0 = y, x1 = x + w, y1 = y + h;
  if(x0 < 0) x0 = 0;
  if(y0 < 0) y0 = 0;
  if(x1 > (s32)width) x1 = (s32)width;
  if(y1 > (s32)height) y1 = (s32)height;
  if(x0 >= x1 || y0 >= y1) return;
  u32 src = (alpha << 24) | rgb;
  for(s32 py = y0; py < y1; py++) {
    auto row = output + py * pitch;
    for(s32 px = x0; px < x1; px++) blendPixelArgb(row[px], src, 255);
  }
}

// Render ASCII text using the 5x7 bitmap font. `maxChars` bounds layout so a
// long title/description cannot spill past the toast box.
static auto drawText5x7(u32* output, u32 pitch, u32 width, u32 height, s32 x, s32 y, const string& text, u32 rgb, u32 alpha, u32 scaleX, u32 scaleY, u32 maxChars) -> u32 {
  u32 drawn = 0;
  s32 cursorX = x;
  for(u32 i : range(text.length())) {
    if(drawn >= maxChars) break;
    auto glyph = glyph5x7(text[i]);
    for(s32 gy : range(7)) {
      u8 row = glyph[gy];
      for(s32 gx : range(5)) {
        if(!(row & (1u << (4 - gx)))) continue;
        fillRectBlend(output, pitch, width, height, cursorX + gx * (s32)scaleX, y + gy * (s32)scaleY, scaleX, scaleY, rgb, alpha);
      }
    }
    cursorX += 5 * (s32)scaleX + (s32)scaleX;
    drawn++;
  }
  return drawn;
}

// Draw a cached ARGB icon into the toast box, scaling it to the target size.
static auto drawIconBlend(u32* output, u32 pitch, u32 width, u32 height, s32 dstX, s32 dstY, s32 dstW, s32 dstH, const std::vector<u32>& icon, u32 iconW, u32 iconH, u32 alpha) -> void {
  if(!output || icon.empty() || iconW == 0 || iconH == 0 || dstW <= 0 || dstH <= 0) return;
  if((u32)icon.size() < iconW * iconH) return;
  for(s32 y = 0; y < dstH; y++) {
    s32 py = dstY + y;
    if(py < 0 || py >= (s32)height) continue;
    auto row = output + py * pitch;
    for(s32 x = 0; x < dstW; x++) {
      s32 px = dstX + x;
      if(px < 0 || px >= (s32)width) continue;
      f64 sx = ((f64)x + 0.5) * (f64)iconW / (f64)dstW - 0.5;
      f64 sy = ((f64)y + 0.5) * (f64)iconH / (f64)dstH - 0.5;
      u32 src = sampleIconBilinear(icon, iconW, iconH, sx, sy);
      blendPixelArgb(row[px], src, alpha);
    }
  }
}

// Pick coarse layout buckets by actual output height. We intentionally keep the
// scale stepped instead of continuous so the toast geometry stays stable while
// resizing the window.
static auto osdScaleForTargetArea(u32 targetHeight) -> u32 {
  if(targetHeight >= 2400) return 4;
  if(targetHeight >= 1600) return 3;
  if(targetHeight >= 900) return 2;
  return 1;
}

static auto rasterizeToastOverlay(
  u32 targetWidth, u32 targetHeight, const Program::Toast& toast, u64 fadeInAgeMs, u64 visibleAgeMs
) -> Program::ToastOverlay {
  // Fast entrance, long readable hold, softer fade-out.
  auto fadeInMs = toast.fadeInDurationMs;
  auto visibleMs = toast.visibleDurationMs;
  auto fadeOutMs = toast.fadeOutDurationMs;
  auto totalLifetimeMs = visibleMs + fadeOutMs;
  Program::ToastOverlay overlay;
  if(targetWidth < 64 || targetHeight < 32) return overlay;
  if(toast.title.length() == 0 && toast.description.length() == 0) return overlay;
  u32 scale = osdScaleForTargetArea(targetHeight);
  // Most geometric constants below are expressed in "font-ish" units and then
  // expanded by this helper. The ~1.5x bump makes the toast more legible than
  // the old status-text sizing without needing a second font.
  auto scalePx = [&](u32 value) -> u32 { return max<u32>(1, (value * 3 + 1) / 2); };

  // Layout recipe:
  // - margin: distance from screen edges
  // - padding: inner box spacing
  // - progress-only toasts can shrink almost entirely to their content width
  // - 20x20 base icon: sized for RA badges/game art
  u32 padX = scalePx(3 * scale);
  u32 padY = scalePx(3 * scale);
  u32 marginX = scalePx(8 * scale);
  u32 marginY = scalePx(8 * scale);
  u32 fontScaleX = scalePx(scale);
  u32 fontScaleY = scalePx(scale);
  u32 charW = 6 * fontScaleX;
  u32 charH = 7 * fontScaleY;
  u32 lineGap = scalePx(2 * scale);
  u32 iconW = scalePx(20 * scale);
  u32 iconH = scalePx(20 * scale);
  bool hasIcon = !toast.icon.empty() && toast.iconWidth && toast.iconHeight;
  u32 contentTextW = max(toast.title.length(), toast.description.length()) * charW;
  u32 nonTextW = padX * 2 + (hasIcon ? iconW + padX : 0);
  u32 minBoxW = toast.kind == Program::ToastKind::Progress
    ? max<u32>(nonTextW + charW * 2, scalePx(48 * scale))
    : scalePx(140 * scale);
  u32 boxH = scalePx(28 * scale);
  u32 preferredBoxW = nonTextW + contentTextW;
  u32 maxBoxW = max<u32>(minBoxW, targetWidth > marginX * 2 ? targetWidth - marginX * 2 : targetWidth);
  u32 boxW = min(maxBoxW, max(minBoxW, preferredBoxW));
  u32 textMaxChars = max<u32>(8, (boxW - nonTextW) / max<u32>(1, charW));

  // Grow the box vertically if we have two text rows and/or an icon taller
  // than the text block.
  u32 contentH = charH;
  if(toast.description.length()) contentH += lineGap + charH;
  boxH = max(boxH, contentH + padY * 2);
  if(hasIcon) boxH = max(boxH, iconH + padY * 2);

  // Convert toast age into a single opacity value used for the whole card.
  u32 alpha = 255;
  if(fadeInMs > 0 && fadeInAgeMs < fadeInMs) {
    alpha = (u32)(255 * fadeInAgeMs / max<u64>(1, fadeInMs));
  } else if(visibleAgeMs > visibleMs) {
    if(fadeOutMs == 0) return overlay;
    auto fadeAge = min<u64>(visibleAgeMs - visibleMs, fadeOutMs);
    alpha = (u32)(255 - (255 * fadeAge / max<u64>(1, fadeOutMs)));
  }
  if(visibleAgeMs >= totalLifetimeMs || alpha == 0) return overlay;

  // Anchor the toast box within a 3x3 grid so callers can pick a stable OSD
  // location without affecting the card layout itself.
  s32 boxX = (s32)marginX;
  s32 boxY = (s32)targetHeight - (s32)marginY - (s32)boxH;
  switch(toast.anchor) {
  case Program::ToastAnchor::TopLeft:
    boxX = (s32)marginX;
    boxY = (s32)marginY;
    break;
  case Program::ToastAnchor::TopCenter:
    boxX = ((s32)targetWidth - (s32)boxW) / 2;
    boxY = (s32)marginY;
    break;
  case Program::ToastAnchor::TopRight:
    boxX = (s32)targetWidth - (s32)marginX - (s32)boxW;
    boxY = (s32)marginY;
    break;
  case Program::ToastAnchor::MiddleLeft:
    boxX = (s32)marginX;
    boxY = ((s32)targetHeight - (s32)boxH) / 2;
    break;
  case Program::ToastAnchor::MiddleCenter:
    boxX = ((s32)targetWidth - (s32)boxW) / 2;
    boxY = ((s32)targetHeight - (s32)boxH) / 2;
    break;
  case Program::ToastAnchor::MiddleRight:
    boxX = (s32)targetWidth - (s32)marginX - (s32)boxW;
    boxY = ((s32)targetHeight - (s32)boxH) / 2;
    break;
  case Program::ToastAnchor::BottomCenter:
    boxX = ((s32)targetWidth - (s32)boxW) / 2;
    boxY = (s32)targetHeight - (s32)marginY - (s32)boxH;
    break;
  case Program::ToastAnchor::BottomRight:
    boxX = (s32)targetWidth - (s32)marginX - (s32)boxW;
    boxY = (s32)targetHeight - (s32)marginY - (s32)boxH;
    break;
  case Program::ToastAnchor::BottomLeft:
    break;
  }
  overlay.width = boxW;
  overlay.height = boxH;
  overlay.x = boxX;
  overlay.y = boxY;
  overlay.pixels.resize((u64)boxW * boxH);
  auto* output = overlay.pixels.data();
  u32 pitch = boxW;
  u32 width = boxW;
  u32 height = boxH;

  fillRectBlend(output, pitch, width, height, 0, 0, boxW, boxH, 0x101010, (alpha * 225) / 255);
  fillRectBlend(output, pitch, width, height, 0, 0, boxW, scalePx(scale), toast.accentColor, (alpha * 245) / 255);

  s32 textX = (s32)padX;
  if(hasIcon) textX += (s32)iconW + (s32)padX;
  s32 titleY = (s32)padY;
  s32 descY = titleY + (s32)charH + (s32)lineGap;

  if(hasIcon) {
    s32 iconY = ((s32)boxH - (s32)iconH) / 2;
    drawIconBlend(output, pitch, width, height, (s32)padX, iconY, iconW, iconH, toast.icon, toast.iconWidth, toast.iconHeight, alpha);
  }

  drawText5x7(output, pitch, width, height, textX, titleY, toast.title, 0xffffff, alpha, fontScaleX, fontScaleY, textMaxChars);
  drawText5x7(output, pitch, width, height, textX, descY, toast.description, 0xd0d0d0, alpha, fontScaleX, fontScaleY, textMaxChars);
  return overlay;
}

}

auto Program::rasterizeToastOverlay(u32 targetWidth, u32 targetHeight, const Toast& toast) -> ToastOverlay {
  if(toast.title.length() == 0 && toast.description.length() == 0) return {};
  auto now = chrono::millisecond();
  auto fadeInAgeMs = now - toast.timestamp;
  auto visibleAgeMs = now - toast.updatedTimestamp;
  return ::rasterizeToastOverlay(targetWidth, targetHeight, toast, fadeInAgeMs, visibleAgeMs);
}

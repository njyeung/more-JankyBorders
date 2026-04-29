#pragma once
#include <CoreGraphics/CoreGraphics.h>
#include <math.h>

#define GRADIENT_MAX_STOPS 16

struct gradient_stop {
  uint32_t color;
  float position;
};

struct gradient {
  float angle;
  struct gradient_stop stops[GRADIENT_MAX_STOPS];
  uint8_t n_stops;
};

static inline void colors_from_hex(uint32_t hex, float* a, float* r, float* g, float* b) {
  *a = ((hex >> 24) & 0xff) / 255.f;
  *r = ((hex >> 16) & 0xff) / 255.f;
  *g = ((hex >> 8) & 0xff) / 255.f;
  *b = ((hex >> 0) & 0xff) / 255.f;
}

static inline void rgb_to_hsv(float r, float g, float b, float* h, float* s, float* v) {
  float max = fmaxf(fmaxf(r, g), b);
  float min = fminf(fminf(r, g), b);
  float d = max - min;
  *v = max;
  *s = (max == 0.f) ? 0.f : d / max;
  if (d == 0.f) {
    *h = 0.f;
  } else if (max == r) {
    *h = ((g - b) / d + (g < b ? 6.f : 0.f)) / 6.f;
  } else if (max == g) {
    *h = ((b - r) / d + 2.f) / 6.f;
  } else {
    *h = ((r - g) / d + 4.f) / 6.f;
  }
}

static inline void hsv_to_rgb(float h, float s, float v, float* r, float* g, float* b) {
  if (s == 0.f) { *r = *g = *b = v; return; }
  h *= 6.f;
  int   i = (int)h % 6;
  float f = h - (int)h;
  float p = v * (1.f - s);
  float q = v * (1.f - s * f);
  float t = v * (1.f - s * (1.f - f));
  switch (i) {
    case 0: *r = v; *g = t; *b = p; break;
    case 1: *r = q; *g = v; *b = p; break;
    case 2: *r = p; *g = v; *b = t; break;
    case 3: *r = p; *g = q; *b = v; break;
    case 4: *r = t; *g = p; *b = v; break;
    case 5: *r = v; *g = p; *b = q; break;
  }
}

static inline void rgb_to_hsl(float r, float g, float b, float* h, float* s, float* l) {
  float max = fmaxf(fmaxf(r, g), b);
  float min = fminf(fminf(r, g), b);
  *l = (max + min) / 2.f;
  if (max == min) {
    *h = *s = 0.f;
  } else {
    float d = max - min;
    *s = (*l > 0.5f) ? d / (2.f - max - min) : d / (max + min);
    if      (max == r) *h = ((g - b) / d + (g < b ? 6.f : 0.f)) / 6.f;
    else if (max == g) *h = ((b - r) / d + 2.f) / 6.f;
    else               *h = ((r - g) / d + 4.f) / 6.f;
  }
}

static inline float hsl_hue_to_rgb(float p, float q, float t) {
  if (t < 0.f) t += 1.f;
  if (t > 1.f) t -= 1.f;
  if (t < 1.f/6.f) return p + (q - p) * 6.f * t;
  if (t < 1.f/2.f) return q;
  if (t < 2.f/3.f) return p + (q - p) * (2.f/3.f - t) * 6.f;
  return p;
}

static inline void hsl_to_rgb(float h, float s, float l, float* r, float* g, float* b) {
  if (s == 0.f) {
    *r = *g = *b = l;
  } else {
    float q = l < 0.5f ? l * (1.f + s) : l + s - l * s;
    float p = 2.f * l - q;
    *r = hsl_hue_to_rgb(p, q, h + 1.f/3.f);
    *g = hsl_hue_to_rgb(p, q, h);
    *b = hsl_hue_to_rgb(p, q, h - 1.f/3.f);
  }
}

// angle=0 == left to right; angle=90 == bottom to top.
// Points are in normalized [0,1]^2 space. Caller applies CGAffineTransform for the actual rect size.
// Endpoints are projected to the square boundary so the full rect is covered at any angle.
static inline void gradient_angle_to_points(float angle_deg, CGPoint dir[2]) {
  float rad   = angle_deg * (float)M_PI / 180.f;
  float cos_a = cosf(rad);
  float sin_a = sinf(rad);
  float t     = 0.5f * (fabsf(cos_a) + fabsf(sin_a));
  dir[0] = CGPointMake(0.5f - t * cos_a, 0.5f - t * sin_a);
  dir[1] = CGPointMake(0.5f + t * cos_a, 0.5f + t * sin_a);
}

static inline void drawing_set_fill(CGContextRef context, uint32_t color) {
  float a,r,g,b;
  colors_from_hex(color, &a, &r, &g, &b);
  CGContextSetRGBFillColor(context, r, g, b, a);
}

static inline void drawing_set_stroke(CGContextRef context, uint32_t color) {
  float a,r,g,b;
  colors_from_hex(color, &a, &r, &g, &b);
  CGContextSetRGBStrokeColor(context, r, g, b, a);
}

static inline void drawing_set_stroke_and_fill(CGContextRef context, uint32_t color, bool glow) {
  float a,r,g,b;
  colors_from_hex(color, &a, &r, &g, &b);
  CGContextSetRGBFillColor(context, r, g, b, a);
  CGContextSetRGBStrokeColor(context, r, g, b, a);

  if (glow) {
    CGColorRef color_ref = CGColorCreateGenericRGB(r, g, b, 1.0);
    CGContextSetShadowWithColor(context, CGSizeZero, 10.0, color_ref);
    CGColorRelease(color_ref);
  }
}

static inline void drawing_clip_between_rect_and_path(CGContextRef context, CGRect frame, CGPathRef path) {
  CGMutablePathRef clip_path = CGPathCreateMutable();
  CGPathAddRect(clip_path, NULL, frame);
  CGPathAddPath(clip_path, NULL, path);
  CGContextAddPath(context, clip_path);
  CGContextEOClip(context);
  CFRelease(clip_path);
}

static inline void drawing_add_rect_with_inset(CGContextRef context, CGRect rect, float inset) {
  CGRect square_rect = CGRectInset(rect, inset, inset);
  CGPathRef square_path = CGPathCreateWithRect(square_rect, NULL);
  CGContextAddPath(context, square_path);
  CFRelease(square_path);
}

static inline void drawing_add_rounded_rect(CGContextRef context, CGRect rect, float border_radius) {
  CGPathRef stroke_path = CGPathCreateWithRoundedRect(rect,
                                                      border_radius,
                                                      border_radius,
                                                      NULL          );

  CGContextAddPath(context, stroke_path);
  CFRelease(stroke_path);
}

static inline void drawing_draw_square_with_inset(CGContextRef context, CGRect rect, float inset) {
  drawing_add_rect_with_inset(context, rect, inset);
  CGContextFillPath(context);
}

static inline void drawing_draw_square_gradient_with_inset(CGContextRef context,CGGradientRef gradient, CGPoint dir[2], CGRect rect, float inset) {
  drawing_add_rect_with_inset(context, rect, inset);
  CGContextClip(context);
  CGContextDrawLinearGradient(context, gradient, dir[0], dir[1], 0);
}

static inline void drawing_draw_rounded_rect_with_inset(CGContextRef context, CGRect rect, float border_radius, bool fill) {
  drawing_add_rounded_rect(context, rect, border_radius);
  if (fill) CGContextFillPath(context);
  else CGContextStrokePath(context);
}

static inline void drawing_draw_rounded_gradient_with_inset(CGContextRef context,CGGradientRef gradient, CGPoint dir[2], CGRect rect, float border_radius) {
  drawing_add_rounded_rect(context, rect, border_radius);
  CGContextReplacePathWithStrokedPath(context);
  CGContextClip(context);
  CGContextDrawLinearGradient(context, gradient, dir[0], dir[1], 0);
}

static inline void drawing_draw_filled_path(CGContextRef context, CGPathRef path, uint32_t color) {
  drawing_set_fill(context, color);
  drawing_set_stroke(context, 0);
  CGContextAddPath(context, path);
  CGContextFillPath(context);
}

static inline CGGradientRef drawing_create_gradient(struct gradient* gradient, CGAffineTransform trans, CGPoint direction[2]) {
  CGColorRef colors[GRADIENT_MAX_STOPS];
  CGFloat    positions[GRADIENT_MAX_STOPS];
  for (int i = 0; i < gradient->n_stops; i++) {
    float a, r, g, b;
    colors_from_hex(gradient->stops[i].color, &a, &r, &g, &b);
    colors[i]    = CGColorCreateSRGB(r, g, b, a);
    positions[i] = gradient->stops[i].position;
  }
  CFArrayRef    cfc    = CFArrayCreate(NULL, (const void**)colors, gradient->n_stops, &kCFTypeArrayCallBacks);
  CGGradientRef result = CGGradientCreateWithColors(NULL, cfc, positions);
  CFRelease(cfc);
  for (int i = 0; i < gradient->n_stops; i++) CGColorRelease(colors[i]);

  gradient_angle_to_points(gradient->angle, direction);
  direction[0] = CGPointApplyAffineTransform(direction[0], trans);
  direction[1] = CGPointApplyAffineTransform(direction[1], trans);
  return result;
}


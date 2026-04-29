#include "parse.h"
#include "border.h"
#include "hashtable.h"

static bool str_starts_with(char* string, char* prefix) {
  if (!string || !prefix) return false;
  if (strlen(string) < strlen(prefix)) return false;
  if (strncmp(prefix, string, strlen(prefix)) == 0) return true;
  return false;
}

static bool parse_list(struct table* list, char* token) {
  uint32_t token_len = strlen(token) + 1;
  char copy[token_len];
  memcpy(copy, token, token_len);

  char* name;
  char* cursor = copy;
  bool entry_found = false;

  table_clear(list);
  while((name = strsep(&cursor, ","))) {
    if (strlen(name) > 0) {
      _table_add(list, name, strlen(name) + 1, (void*)true);
      entry_found = true;
    }
  }
  return entry_found;
}

// Parses the interior of gradient(...): angle first, then color stops.
// Each stop is either 0xAARRGGBB or 0xAARRGGBB@position.
// Stops without an explicit position are evenly distributed across 0..1.
static bool parse_gradient_new(struct color_style* style, char* content) {
  uint32_t content_len = strlen(content) + 1;
  char buf[content_len];
  memcpy(buf, content, content_len);

  style->stype          = COLOR_STYLE_GRADIENT;
  style->gradient.n_stops = 0;
  style->gradient.angle   = 0.f;

  char* cursor = buf;
  char* part;
  bool  angle_parsed = false;

  while ((part = strsep(&cursor, ","))) {
    if (strlen(part) == 0) continue;
    if (!angle_parsed) {
      style->gradient.angle = strtof(part, NULL);
      angle_parsed = true;
      continue;
    }
    if (style->gradient.n_stops >= GRADIENT_MAX_STOPS) break;
    int      n = style->gradient.n_stops;
    uint32_t color;
    float    pos;
    if (sscanf(part, "0x%x@%f", &color, &pos) == 2) {
      style->gradient.stops[n] = (struct gradient_stop){ color, pos };
    } else if (sscanf(part, "0x%x", &color) == 1) {
      style->gradient.stops[n] = (struct gradient_stop){ color, -1.f };
    } else {
      return false;
    }
    style->gradient.n_stops++;
  }

  if (style->gradient.n_stops < 2) return false;

  // Evenly distribute any stops that had no explicit position
  for (int i = 0; i < style->gradient.n_stops; i++) {
    if (style->gradient.stops[i].position < 0.f)
      style->gradient.stops[i].position = (float)i / (float)(style->gradient.n_stops - 1);
  }
  return true;
}

static bool parse_color(struct color_style* style, char* token) {
  uint32_t hex;
  if (sscanf(token, "=0x%x", &hex) == 1) {
    style->stype = COLOR_STYLE_GRADIENT;
    style->gradient.angle    = 0.f;
    style->gradient.n_stops  = 1;
    style->gradient.stops[0] = (struct gradient_stop){ hex, 0.f };
    return true;
  }
  if (sscanf(token, "=glow(0x%x)", &hex) == 1) {
    style->stype = COLOR_STYLE_GLOW;
    style->gradient.angle    = 0.f;
    style->gradient.n_stops  = 1;
    style->gradient.stops[0] = (struct gradient_stop){ hex, 0.f };
    return true;
  }
  // Angle-based multi-stop: gradient(<angle>,<color>,...[,<color>@<pos>,...])
  static const char grad_prefix[] = "=gradient(";
  static const uint32_t grad_prefix_len = sizeof(grad_prefix) - 1;
  if (strlen(token) > grad_prefix_len
      && strncmp(token, grad_prefix, grad_prefix_len) == 0) {
    char* inner = token + grad_prefix_len;
    char* close = strrchr(inner, ')');
    if (close) {
      *close = '\0';
      if (parse_gradient_new(style, inner)) return true;
    }
  }
  printf("[?] Borders: Invalid color argument color%s\n", token);
  return false;
}

uint32_t parse_settings(struct settings* settings, int count, char** arguments) {
  static char active_color[] = "active_color";
  static char inactive_color[] = "inactive_color";
  static char background_color[] = "background_color";
  static char blacklist[] = "blacklist=";
  static char whitelist[] = "whitelist=";

  char order = 'a';
  uint32_t update_mask = 0;
  for (int i = 0; i < count; i++) {
    if (str_starts_with(arguments[i], active_color)) {
      if (parse_color(&settings->active_window,
                                 arguments[i] + strlen(active_color))) {
        update_mask |= BORDER_UPDATE_MASK_ACTIVE;
      }
    }
    else  if (str_starts_with(arguments[i], inactive_color)) {
      if (parse_color(&settings->inactive_window,
                                 arguments[i] + strlen(inactive_color))) {
        update_mask |= BORDER_UPDATE_MASK_INACTIVE;
      }
    }
    else if (str_starts_with(arguments[i], background_color)) {
      if (parse_color(&settings->background,
                                 arguments[i] + strlen(background_color))) {
        update_mask |= BORDER_UPDATE_MASK_ALL;
        settings->show_background = settings->background.gradient.stops[0].color & 0xff000000;
      }
    }
    else if (str_starts_with(arguments[i], blacklist)) {
      settings->blacklist_enabled = parse_list(&settings->blacklist,
                                               arguments[i]
                                               + strlen(blacklist));
      update_mask |= BORDER_UPDATE_MASK_RECREATE_ALL;
    }
    else if (str_starts_with(arguments[i], whitelist)) {
      settings->whitelist_enabled = parse_list(&settings->whitelist,
                                               arguments[i]
                                               + strlen(whitelist));
      update_mask |= BORDER_UPDATE_MASK_RECREATE_ALL;
    }
    else if (sscanf(arguments[i], "width=%f", &settings->border_width) == 1) {
      update_mask |= BORDER_UPDATE_MASK_ALL;
    }
    else if (sscanf(arguments[i], "order=%c", &order) == 1) {
      if (order == 'a') settings->border_order = BORDER_ORDER_ABOVE;
      else settings->border_order = BORDER_ORDER_BELOW;
      update_mask |= BORDER_UPDATE_MASK_ALL;
    }
    else if (sscanf(arguments[i], "style=%c", &settings->border_style) == 1) {
      update_mask |= BORDER_UPDATE_MASK_ALL;
    }
    else if (strcmp(arguments[i], "hidpi=on") == 0) {
      update_mask |= BORDER_UPDATE_MASK_RECREATE_ALL;
      settings->hidpi = true;
    }
    else if (strcmp(arguments[i], "hidpi=off") == 0) {
      update_mask |= BORDER_UPDATE_MASK_RECREATE_ALL;
      settings->hidpi = false;
    }
    else if (strcmp(arguments[i], "ax_focus=on") == 0) {
      settings->ax_focus = true;
      update_mask |= BORDER_UPDATE_MASK_SETTING;
    }
    else if (strcmp(arguments[i], "ax_focus=off") == 0) {
      settings->ax_focus = false;
      update_mask |= BORDER_UPDATE_MASK_SETTING;
    }
    else if (sscanf(arguments[i], "apply-to=%d", &settings->apply_to) == 1) {
      update_mask |= BORDER_UPDATE_MASK_SETTING;
    }
    else {
      printf("[?] Borders: Invalid argument '%s'\n", arguments[i]);
    }
  }
  return update_mask;
}

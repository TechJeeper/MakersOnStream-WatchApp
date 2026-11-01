#include "ui.h"
#include "data.h"
#include "comm.h"

static Window *s_main;
static Window *s_detail;
static MenuLayer *s_menu;
static StatusBarLayer *s_status;
static TextLayer *s_detail_name;
static TextLayer *s_detail_sub;
static TextLayer *s_detail_meta;
static TextLayer *s_detail_hint;
static ScrollLayer *s_detail_scroll;
static const MosItem *s_selected;

static GColor color_bg(void) {
#ifdef PBL_COLOR
  return GColorBlack;
#else
  return GColorBlack;
#endif
}

static GColor color_live(void) {
#ifdef PBL_COLOR
  return GColorRed;
#else
  return GColorWhite;
#endif
}

static GColor color_soon(void) {
#ifdef PBL_COLOR
  return GColorVividCerulean;
#else
  return GColorWhite;
#endif
}

static GColor color_muted(void) {
#ifdef PBL_COLOR
  return GColorLightGray;
#else
  return GColorWhite;
#endif
}

static uint16_t menu_num_sections(MenuLayer *layer, void *ctx) {
  (void)layer;
  (void)ctx;
  return data_section_count();
}

static uint16_t menu_num_rows(MenuLayer *layer, uint16_t section, void *ctx) {
  (void)layer;
  (void)ctx;
  return data_rows_in_section((uint8_t)section);
}

static int16_t menu_header_height(MenuLayer *layer, uint16_t section, void *ctx) {
  (void)layer;
  (void)section;
  (void)ctx;
  return 22;
}

static void menu_draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *data) {
  (void)data;
  MosData *d = data_get();
  char title[24];
  GColor accent = section == 0 ? color_live() : color_soon();

  if (section == 0) {
    if (d->state == MOS_STATE_READY) {
      snprintf(title, sizeof(title), "LIVE · %d", (int)d->live_count);
    } else {
      snprintf(title, sizeof(title), "LIVE");
    }
  } else {
    if (d->state == MOS_STATE_READY) {
      snprintf(title, sizeof(title), "UP NEXT · %d", (int)d->soon_count);
    } else {
      snprintf(title, sizeof(title), "UP NEXT");
    }
  }

  graphics_context_set_fill_color(ctx, color_bg());
  graphics_fill_rect(ctx, layer_get_bounds(cell), 0, GCornerNone);

  graphics_context_set_fill_color(ctx, accent);
  graphics_fill_rect(ctx, GRect(0, 6, 3, 12), 0, GCornerNone);

  graphics_context_set_text_color(ctx, accent);
  graphics_draw_text(ctx, title,
                     fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(10, 2, layer_get_bounds(cell).size.w - 14, 18),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
}

static void draw_placeholder_row(GContext *ctx, const Layer *cell, const char *text) {
  graphics_context_set_text_color(ctx, color_muted());
  graphics_draw_text(ctx, text,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18),
                     GRect(8, 8, layer_get_bounds(cell).size.w - 16, 28),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
}

static void menu_draw_row(GContext *ctx, const Layer *cell, MenuIndex *index, void *data) {
  (void)data;
  MosData *d = data_get();
  GRect bounds = layer_get_bounds(cell);

  if (d->state != MOS_STATE_READY) {
    if (index->section == 0) {
      draw_placeholder_row(ctx, cell, d->status[0] ? d->status : "Loading…");
    }
    return;
  }

  const MosItem *item = data_item_at((uint8_t)index->section, (uint8_t)index->row);
  if (!item || item->name[0] == '\0') {
    if (index->section == 0) {
      draw_placeholder_row(ctx, cell, "Nobody live");
    } else {
      draw_placeholder_row(ctx, cell, "Nothing soon");
    }
    return;
  }

  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, item->name,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(8, 0, bounds.size.w - 16, 22),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);

  const char *sub = item->subtitle[0] ? item->subtitle : item->when;
  graphics_context_set_text_color(ctx, color_muted());
  graphics_draw_text(ctx, sub,
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(8, 22, bounds.size.w - 16, 18),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
}

static void detail_load(const MosItem *item) {
  s_selected = item;
  static char meta[80];
  if (item->kind == MOS_KIND_LIVE) {
    snprintf(meta, sizeof(meta), "LIVE  ·  %s",
             item->platform[0] ? item->platform : "Streaming");
  } else {
    snprintf(meta, sizeof(meta), "%s  ·  %s",
             item->when[0] ? item->when : "Soon",
             item->platform[0] ? item->platform : "Stream");
  }
  text_layer_set_text(s_detail_name, item->name);
  text_layer_set_text(s_detail_sub, item->subtitle[0] ? item->subtitle : " ");
  text_layer_set_text(s_detail_meta, meta);
}

static void menu_select(MenuLayer *layer, MenuIndex *index, void *ctx) {
  (void)layer;
  (void)ctx;
  MosData *d = data_get();
  if (d->state != MOS_STATE_READY) {
    comm_request_refresh();
    return;
  }
  const MosItem *item = data_item_at((uint8_t)index->section, (uint8_t)index->row);
  if (!item || item->name[0] == '\0') {
    comm_request_refresh();
    return;
  }
  detail_load(item);
  window_stack_push(s_detail, true);
}

static void menu_select_long(MenuLayer *layer, MenuIndex *index, void *ctx) {
  (void)layer;
  (void)index;
  (void)ctx;
  comm_request_refresh();
}

static void main_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_status = status_bar_layer_create();
  status_bar_layer_set_colors(s_status, color_bg(), GColorWhite);
  status_bar_layer_set_separator_mode(s_status, StatusBarLayerSeparatorModeNone);
  layer_add_child(root, status_bar_layer_get_layer(s_status));

  GRect menu_bounds = GRect(0, STATUS_BAR_LAYER_HEIGHT,
                            bounds.size.w, bounds.size.h - STATUS_BAR_LAYER_HEIGHT);
  s_menu = menu_layer_create(menu_bounds);
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
    .get_num_sections = menu_num_sections,
    .get_num_rows = menu_num_rows,
    .get_header_height = menu_header_height,
    .draw_header = menu_draw_header,
    .draw_row = menu_draw_row,
    .select_click = menu_select,
    .select_long_click = menu_select_long
  });
  menu_layer_set_click_config_onto_window(s_menu, window);
#ifdef PBL_COLOR
  menu_layer_set_normal_colors(s_menu, color_bg(), GColorWhite);
  menu_layer_set_highlight_colors(s_menu, GColorDarkGray, GColorWhite);
#endif
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void main_window_unload(Window *window) {
  (void)window;
  menu_layer_destroy(s_menu);
  status_bar_layer_destroy(s_status);
}

static void detail_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_detail_scroll = scroll_layer_create(bounds);
  scroll_layer_set_click_config_onto_window(s_detail_scroll, window);
  layer_add_child(root, scroll_layer_get_layer(s_detail_scroll));

  s_detail_meta = text_layer_create(GRect(10, 8, bounds.size.w - 20, 22));
  text_layer_set_font(s_detail_meta, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_color(s_detail_meta, color_live());
  text_layer_set_background_color(s_detail_meta, GColorClear);
  scroll_layer_add_child(s_detail_scroll, text_layer_get_layer(s_detail_meta));

  s_detail_name = text_layer_create(GRect(10, 28, bounds.size.w - 20, 56));
  text_layer_set_font(s_detail_name, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_color(s_detail_name, GColorWhite);
  text_layer_set_background_color(s_detail_name, GColorClear);
  text_layer_set_overflow_mode(s_detail_name, GTextOverflowModeWordWrap);
  scroll_layer_add_child(s_detail_scroll, text_layer_get_layer(s_detail_name));

  s_detail_sub = text_layer_create(GRect(10, 88, bounds.size.w - 20, 80));
  text_layer_set_font(s_detail_sub, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_color(s_detail_sub, color_muted());
  text_layer_set_background_color(s_detail_sub, GColorClear);
  text_layer_set_overflow_mode(s_detail_sub, GTextOverflowModeWordWrap);
  scroll_layer_add_child(s_detail_scroll, text_layer_get_layer(s_detail_sub));

  s_detail_hint = text_layer_create(GRect(10, bounds.size.h - 28, bounds.size.w - 20, 20));
  text_layer_set_font(s_detail_hint, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_color(s_detail_hint, color_muted());
  text_layer_set_background_color(s_detail_hint, GColorClear);
  text_layer_set_text(s_detail_hint, "Back · long-Select refresh");
  layer_add_child(root, text_layer_get_layer(s_detail_hint));

  scroll_layer_set_content_size(s_detail_scroll, GSize(bounds.size.w, 180));
}

static void detail_window_unload(Window *window) {
  (void)window;
  text_layer_destroy(s_detail_name);
  text_layer_destroy(s_detail_sub);
  text_layer_destroy(s_detail_meta);
  text_layer_destroy(s_detail_hint);
  scroll_layer_destroy(s_detail_scroll);
  s_selected = NULL;
}

void ui_reload(void) {
  if (s_menu) {
    menu_layer_reload_data(s_menu);
  }
}

void ui_init(void) {
  s_main = window_create();
  window_set_background_color(s_main, color_bg());
  window_set_window_handlers(s_main, (WindowHandlers){
    .load = main_window_load,
    .unload = main_window_unload
  });

  s_detail = window_create();
  window_set_background_color(s_detail, color_bg());
  window_set_window_handlers(s_detail, (WindowHandlers){
    .load = detail_window_load,
    .unload = detail_window_unload
  });

  window_stack_push(s_main, true);
}

void ui_deinit(void) {
  window_destroy(s_detail);
  window_destroy(s_main);
}

#include <pebble.h>

static const int FONT_HEIGHT = 44;

static Window *s_window;
static TextLayer *s_text_layer;

static int count;
static char buf[8];

static void print_count() {
  snprintf(buf, sizeof(buf), "%04d", count);
  text_layer_set_text(s_text_layer, buf);
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  count = 0;
  print_count();
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  count = count + 1;
  print_count();
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  count = count - 1;
  print_count();
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  int x = 0;
  int y = (bounds.size.h / 2) - (FONT_HEIGHT / 2);
  int w = bounds.size.w;
  int h = FONT_HEIGHT;

  s_text_layer = text_layer_create(GRect(x, y, w, h));
  text_layer_set_text_alignment(s_text_layer, GTextAlignmentCenter);
  text_layer_set_font(s_text_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT));
  layer_add_child(window_layer, text_layer_get_layer(s_text_layer));

  print_count();
}

static void prv_window_unload(Window *window) {
  text_layer_destroy(s_text_layer);
}

static void prv_init(void) {
  s_window = window_create();
  window_set_click_config_provider(s_window, prv_click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  const bool animated = true;
  window_stack_push(s_window, animated);
}

static void prv_deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  prv_init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", s_window);

  app_event_loop();
  prv_deinit();
}

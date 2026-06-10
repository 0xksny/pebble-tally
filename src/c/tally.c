#include <pebble.h>

#define DIGIT_COUNT 4
#define FLIP_DURATION_MS 360
#define FRAME_INTERVAL_MS 30

#define MIN_COUNT 0
#define MAX_COUNT 9999

#define TEXT_LAYER_Y_OFFSET 4

typedef struct {
  Layer *card_layer;
  Layer *top_clip_layer;
  Layer *bottom_clip_layer;
  Layer *flap_clip_layer;
  Layer *hinge_layer;
  TextLayer *old_top_text;
  TextLayer *new_bottom_text;
  TextLayer *flap_text;
  char old_character[2];
  char new_character[2];
  char flap_character[2];
  int16_t width;
  int16_t height;
} DigitCard;

static Window *s_window;
static Layer *s_counter_layer;
static AppTimer *s_animation_timer;
static GFont s_digit_font;
static DigitCard s_cards[DIGIT_COUNT];

static int s_count;
static int s_displayed_count;
static int s_animation_from_count;
static int s_animation_to_count;
static uint16_t s_animation_elapsed_ms;
static bool s_animating;

static GColor prv_card_color(void) {
  return PBL_IF_COLOR_ELSE(GColorOxfordBlue, GColorBlack);
}

static void prv_format_count(int count, char output[DIGIT_COUNT + 1]) {
  snprintf(output, DIGIT_COUNT + 1, "%04d", count);
}

static void prv_clip_layer_update(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, prv_card_color());
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

static TextLayer *prv_create_digit_text_layer(GRect frame) {
  TextLayer *text_layer = text_layer_create(frame);
  text_layer_set_background_color(text_layer, GColorClear);
  text_layer_set_text_color(text_layer, GColorWhite);
  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
  text_layer_set_font(text_layer, s_digit_font);
  return text_layer;
}

static void prv_hinge_layer_update(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  graphics_context_set_stroke_color(
      ctx, PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite));
  graphics_draw_line(ctx, GPoint(0, 0), GPoint(bounds.size.w - 1, 0));
}

static void prv_create_card(DigitCard *card, Layer *parent, GRect frame) {
  const int16_t half_height = frame.size.h / 2;

  card->width = frame.size.w;
  card->height = frame.size.h;
  card->old_character[1] = '\0';
  card->new_character[1] = '\0';
  card->flap_character[1] = '\0';

  card->card_layer = layer_create(frame);
  layer_add_child(parent, card->card_layer);

  card->top_clip_layer = layer_create(GRect(0, 0, card->width, half_height));
  card->bottom_clip_layer = layer_create(
      GRect(0, half_height, card->width, card->height - half_height));
  card->flap_clip_layer =
      layer_create(GRect(0, half_height, card->width, half_height));
  layer_set_clips(card->top_clip_layer, true);
  layer_set_clips(card->bottom_clip_layer, true);
  layer_set_clips(card->flap_clip_layer, true);
  layer_set_update_proc(card->top_clip_layer, prv_clip_layer_update);
  layer_set_update_proc(card->bottom_clip_layer, prv_clip_layer_update);
  layer_set_update_proc(card->flap_clip_layer, prv_clip_layer_update);

  card->old_top_text =
      prv_create_digit_text_layer(GRect(0, TEXT_LAYER_Y_OFFSET, card->width, card->height));
  card->new_bottom_text = prv_create_digit_text_layer(
      GRect(0, -half_height + TEXT_LAYER_Y_OFFSET, card->width, card->height));
  card->flap_text = prv_create_digit_text_layer(
      GRect(0, -half_height + TEXT_LAYER_Y_OFFSET, card->width, card->height));

  layer_add_child(card->top_clip_layer,
                  text_layer_get_layer(card->old_top_text));
  layer_add_child(card->bottom_clip_layer,
                  text_layer_get_layer(card->new_bottom_text));
  layer_add_child(card->flap_clip_layer, text_layer_get_layer(card->flap_text));
  layer_add_child(card->card_layer, card->top_clip_layer);
  layer_add_child(card->card_layer, card->bottom_clip_layer);
  layer_add_child(card->card_layer, card->flap_clip_layer);

  card->hinge_layer = layer_create(GRect(0, half_height, card->width, 1));
  layer_set_update_proc(card->hinge_layer, prv_hinge_layer_update);
  layer_add_child(card->card_layer, card->hinge_layer);
}

static void prv_destroy_card(DigitCard *card) {
  text_layer_destroy(card->old_top_text);
  text_layer_destroy(card->new_bottom_text);
  text_layer_destroy(card->flap_text);
  layer_destroy(card->top_clip_layer);
  layer_destroy(card->bottom_clip_layer);
  layer_destroy(card->flap_clip_layer);
  layer_destroy(card->hinge_layer);
  layer_destroy(card->card_layer);
}

static void prv_set_card_characters(DigitCard *card, char old_character,
                                    char new_character) {
  card->old_character[0] = old_character;
  card->new_character[0] = new_character;
  text_layer_set_text(card->old_top_text, card->old_character);
  text_layer_set_text(card->new_bottom_text, card->new_character);
}

static void prv_set_card_progress(DigitCard *card, uint16_t progress) {
  const int16_t half_height = card->height / 2;

  if (card->old_character[0] == card->new_character[0] || progress >= 1000) {
    layer_set_hidden(card->flap_clip_layer, true);
    return;
  }

  layer_set_hidden(card->flap_clip_layer, false);
  if (progress < 500) {
    const int16_t flap_height = half_height * (500 - progress) / 500;
    layer_set_frame(card->flap_clip_layer,
                    GRect(0, half_height, card->width, flap_height));
    layer_set_frame(text_layer_get_layer(card->flap_text),
                    GRect(0, -half_height + TEXT_LAYER_Y_OFFSET, card->width, card->height));
    card->flap_character[0] = card->old_character[0];
  } else {
    const int16_t flap_height = half_height * (progress - 500) / 500;
    const int16_t flap_y = half_height - flap_height;
    layer_set_frame(card->flap_clip_layer,
                    GRect(0, flap_y, card->width, flap_height));
    layer_set_frame(text_layer_get_layer(card->flap_text),
                    GRect(0, -flap_y + TEXT_LAYER_Y_OFFSET, card->width, card->height));
    card->flap_character[0] = card->new_character[0];
  }
  text_layer_set_text(card->flap_text, card->flap_character);
}

static void prv_update_counter_cards(void) {
  char old_text[DIGIT_COUNT + 1];
  char new_text[DIGIT_COUNT + 1];
  const uint16_t progress =
      s_animating ? (uint32_t)s_animation_elapsed_ms * 1000 / FLIP_DURATION_MS
                  : 1000;

  prv_format_count(s_animating ? s_animation_from_count : s_displayed_count,
                   old_text);
  prv_format_count(s_animating ? s_animation_to_count : s_displayed_count,
                   new_text);

  for (int i = 0; i < DIGIT_COUNT; ++i) {
    prv_set_card_characters(&s_cards[i], old_text[i], new_text[i]);
    prv_set_card_progress(&s_cards[i], progress);
  }
}

static void prv_start_animation_if_needed(void);

static void prv_animation_timer_callback(void *context) {
  s_animation_timer = NULL;
  s_animation_elapsed_ms += FRAME_INTERVAL_MS;

  if (s_animation_elapsed_ms >= FLIP_DURATION_MS) {
    s_animation_elapsed_ms = FLIP_DURATION_MS;
    s_animating = false;
    s_displayed_count = s_animation_to_count;
    prv_update_counter_cards();
    prv_start_animation_if_needed();
    return;
  }

  prv_update_counter_cards();
  s_animation_timer =
      app_timer_register(FRAME_INTERVAL_MS, prv_animation_timer_callback, NULL);
}

static void prv_start_animation_if_needed(void) {
  if (s_animating || s_displayed_count == s_count || !s_counter_layer) {
    return;
  }

  s_animation_from_count = s_displayed_count;
  s_animation_to_count = s_count;
  s_animation_elapsed_ms = 0;
  s_animating = true;
  prv_update_counter_cards();
  s_animation_timer =
      app_timer_register(FRAME_INTERVAL_MS, prv_animation_timer_callback, NULL);
}

static void prv_select_click_handler(ClickRecognizerRef recognizer,
                                     void *context) {
  s_count = 0;
  prv_start_animation_if_needed();
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_count < MAX_COUNT) {
    s_count++;
  }
  prv_start_animation_if_needed();
}

static void prv_down_click_handler(ClickRecognizerRef recognizer,
                                   void *context) {
  if (s_count > MIN_COUNT) {
    s_count--;
  }
  prv_start_animation_if_needed();
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(window_layer);
  const int16_t gap = 3;
  const int16_t horizontal_margin = PBL_IF_ROUND_ELSE(14, 6);
  const int16_t available_width =
      bounds.size.w - horizontal_margin * 2 - gap * (DIGIT_COUNT - 1);
  const int16_t card_width = available_width / DIGIT_COUNT;
  const int16_t card_height = 62;
  const int16_t total_width =
      card_width * DIGIT_COUNT + gap * (DIGIT_COUNT - 1);
  const int16_t start_x = (bounds.size.w - total_width) / 2;
  const int16_t start_y = (bounds.size.h - card_height) / 2;

  s_digit_font = fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
  s_counter_layer = layer_create(bounds);
  layer_add_child(window_layer, s_counter_layer);

  for (int i = 0; i < DIGIT_COUNT; ++i) {
    prv_create_card(&s_cards[i], s_counter_layer,
                    GRect(start_x + i * (card_width + gap), start_y, card_width,
                          card_height));
  }
  prv_update_counter_cards();
}

static void prv_window_unload(Window *window) {
  if (s_animation_timer) {
    app_timer_cancel(s_animation_timer);
    s_animation_timer = NULL;
  }
  for (int i = 0; i < DIGIT_COUNT; ++i) {
    prv_destroy_card(&s_cards[i]);
  }
  layer_destroy(s_counter_layer);
  s_counter_layer = NULL;
}

static void prv_init(void) {
  s_window = window_create();
  window_set_background_color(s_window,
                              PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
  window_set_click_config_provider(s_window, prv_click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers){
                                           .load = prv_window_load,
                                           .unload = prv_window_unload,
                                       });
  window_stack_push(s_window, true);
}

static void prv_deinit(void) { window_destroy(s_window); }

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}

/*

  Bold Hour watchface with Dates

  Author: Bo Yang

  ===============================

  [Based on]:

   Bold Hour watch

   A digital watch with very large hour digits that take up the entire screen
   and smaller minute digits that fit in the null space of the hour digits.

   This watch's load/unload code is mostly taken from the big_time watchface which has to
   load/unload images as necessary. The same is true for bold-hour.

   24 hour clock not supported

   Author: Jon Eisen <jon@joneisen.me>

 */

#include <pebble.h>

// -- build configuration --
// #define WHITE_TEXT

// -- macros --
#define UNINITTED -1

#ifdef WHITE_TEXT
  #define TEXT_COLOR GColorWhite
  #define BKGD_COLOR GColorBlack
#else
  #define TEXT_COLOR GColorBlack
  #define BKGD_COLOR GColorWhite
#endif

// -- Global assets --
Window *window;

BitmapLayer *hourLayer;
BitmapLayer *batteryLogoLayer;
TextLayer *batteryPercentLayer;
TextLayer *minuteLayer;
TextLayer *dateLayer;
TextLayer *dayLayer;
Layer *bottomBarLayer;

GRect minuteFrame;
GBitmap *hourImage;
GBitmap *batteryLogo;

int loaded_hour;
int loaded_charging_battery = -1;

char last_date_n_day_text[] = "xxx 00xxx";
char last_battery_text[] = "000";

bool initialized = false;
bool bluetooth_connected = true;
int bluetooth_disconnect_anim_repeat = 0;

AppTimer *timer;

// These are all of our images. Each is the entire screen in size.
const int IMAGE_RESOURCE_IDS[12] = {
  RESOURCE_ID_IMAGE_NUM_1, RESOURCE_ID_IMAGE_NUM_2,
  RESOURCE_ID_IMAGE_NUM_3, RESOURCE_ID_IMAGE_NUM_4, RESOURCE_ID_IMAGE_NUM_5,
  RESOURCE_ID_IMAGE_NUM_6, RESOURCE_ID_IMAGE_NUM_7, RESOURCE_ID_IMAGE_NUM_8,
  RESOURCE_ID_IMAGE_NUM_9, RESOURCE_ID_IMAGE_NUM_10, RESOURCE_ID_IMAGE_NUM_11,
  RESOURCE_ID_IMAGE_NUM_12
};


void load_digit_image(int digit_value) {
  if ((digit_value < 1) || (digit_value > 12)) {
    return;
  }

  if (!hourImage) {
    hourImage = gbitmap_create_with_resource(IMAGE_RESOURCE_IDS[digit_value-1]);
    bitmap_layer_set_bitmap(hourLayer, hourImage);
    loaded_hour = digit_value;
  }

}

void unload_digit_image() {

  if (hourImage) {
    gbitmap_destroy(hourImage);
    hourImage = 0;
    loaded_hour = 0;
  }

}

void set_minute_layer_location(unsigned short horiz) {
  if (minuteFrame.origin.x != horiz) {
    minuteFrame.origin.x = horiz;
    layer_set_frame(text_layer_get_layer(minuteLayer), minuteFrame);
    layer_mark_dirty(text_layer_get_layer(minuteLayer));
  }
}

void bottom_bar_layer_update_callback(Layer *layer, GContext* ctx) {
  if(bluetooth_connected) {
    graphics_context_set_stroke_color(ctx, TEXT_COLOR);
    GRect boundary = layer_get_bounds(layer);
    graphics_draw_line(ctx, boundary.origin, GPoint(boundary.origin.x + boundary.size.w, boundary.origin.y));
  } else {
    if(bluetooth_disconnect_anim_repeat % 2 == 1) {
      graphics_context_set_stroke_color(ctx, BKGD_COLOR);  
    } else {
      graphics_context_set_stroke_color(ctx, TEXT_COLOR);
    }
    graphics_draw_rect(ctx, layer_get_bounds(layer));
  }
}

void bluetooh_disconnect_animation_timer_callback(void *context) {
  if(bluetooth_disconnect_anim_repeat-- >= 0) {
    layer_mark_dirty(bottomBarLayer);
    timer = app_timer_register(200, bluetooh_disconnect_animation_timer_callback, NULL);
  }
}

void display_time(struct tm * tick_time) {

  // 24 hour clock not supported

  char new_date_n_day_text[] = "xxx 00xxx"; // size: 9
  
  strftime(new_date_n_day_text, sizeof(new_date_n_day_text), "%b %e%a", tick_time);
  
  if(strcmp(last_date_n_day_text, new_date_n_day_text)) {
    strcpy(last_date_n_day_text, new_date_n_day_text);
    static char new_date_text[] = "xxx 00";
    static char new_day_text[] = "xxx";
    for(int i = 0; i < 9; i++) {
      if(i < 6) {
        new_date_text[i] = new_date_n_day_text[i];
      } else {
        new_day_text[i-6] = new_date_n_day_text[i];
      }
    }
    text_layer_set_text(dateLayer, new_date_text);
    text_layer_set_text(dayLayer, new_day_text);
  }

  unsigned short hour = tick_time->tm_hour % 12;

  // Converts "0" to "12"
  hour = hour ? hour : 12;

  // Only do memory unload/load if necessary
  if (loaded_hour != hour) {
    unload_digit_image();
    load_digit_image(hour);
  }

  // Show minute
  static char text[] = "00";

  strftime(text, sizeof(text), "%M", tick_time);

  unsigned short n1s = (text[0]=='1') + (text[1]=='1');

  if (hour == 10 || hour == 12) {
    set_minute_layer_location(70 + 3*n1s);
  } else {
    set_minute_layer_location(53 + 3*n1s);
  }

  text_layer_set_text(minuteLayer, text);
}

void unload_battery_image() {
  if(batteryLogo){
    gbitmap_destroy(batteryLogo);
    batteryLogo = 0;
  }
}

void update_battery_image(BatteryChargeState state) {
  if (state.is_charging && (!loaded_charging_battery || loaded_charging_battery == -1)) {
    loaded_charging_battery = 1;
    unload_battery_image();
    batteryLogo = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_CHARGE);
    bitmap_layer_set_bitmap(batteryLogoLayer, batteryLogo);
  }

  if(!state.is_charging && (loaded_charging_battery || loaded_charging_battery == -1)) {
    loaded_charging_battery = 0;
    unload_battery_image();
    batteryLogo = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY);
    bitmap_layer_set_bitmap(batteryLogoLayer, batteryLogo);
  }
}

static void handle_battery(BatteryChargeState charge_state) {
  char battery_text[] = "000";
  snprintf(battery_text, sizeof(battery_text), "%d", charge_state.charge_percent);
  if(strcmp(last_battery_text, battery_text)) {
    strcpy(last_battery_text, battery_text);
    text_layer_set_text(batteryPercentLayer, last_battery_text);
  }
  update_battery_image(charge_state);
}

static void handle_bluetooth(bool connected) {
  if(connected != bluetooth_connected) {  
    bluetooth_connected = connected;
    bluetooth_disconnect_anim_repeat = 0;
    layer_mark_dirty(bottomBarLayer);
    
    if(initialized && !connected) {
      vibes_long_pulse();
      bluetooth_disconnect_anim_repeat = 4;
      timer = app_timer_register(200, bluetooh_disconnect_animation_timer_callback, NULL);
    }
  }
}

void handle_minute_tick(struct tm * tick_time, TimeUnits units_changed) {
  display_time(tick_time);
  handle_battery(battery_state_service_peek());
}

void handle_init() {

  // Configure window
  window = window_create();
  window_stack_push(window, true /* Animated */);
  window_set_background_color(window, BKGD_COLOR);
  window_set_fullscreen(window, true);
  Layer *window_layer = window_get_root_layer(window);

  // Dynamic allocation of assets
  minuteFrame = GRect(53, 16, 40, 40);
  minuteLayer = text_layer_create(minuteFrame);
  hourLayer = bitmap_layer_create(GRect(0, 0, 144, 148));
  batteryLogoLayer = bitmap_layer_create(GRect(65, 151, 10, 15));
  batteryPercentLayer = text_layer_create(GRect(78, 150, 30, 167-150));
  dateLayer = text_layer_create(GRect(3, 150, 38, 167-150));
  dayLayer = text_layer_create(GRect(141-30, 150, 30, 167-150));
  text_layer_set_text_alignment(dayLayer, GTextAlignmentRight);
  bottomBarLayer = layer_create(GRect(1, 149, 142, 18));

  // Setup minute layer
  text_layer_set_text_color(minuteLayer, TEXT_COLOR);
  text_layer_set_background_color(minuteLayer, GColorClear);
  text_layer_set_font(minuteLayer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_MINUTE_38)));

  // Setup date & day layer
  text_layer_set_text_color(dateLayer, TEXT_COLOR);
  text_layer_set_background_color(dateLayer, GColorClear);
  text_layer_set_text_color(dayLayer, TEXT_COLOR);
  text_layer_set_background_color(dayLayer, GColorClear);

  // Setup battery layers
  text_layer_set_text_color(batteryPercentLayer, TEXT_COLOR);
  text_layer_set_background_color(batteryPercentLayer, GColorClear);

  // Setup line layer
  layer_set_update_proc(bottomBarLayer, bottom_bar_layer_update_callback);

  // Add layers into hierachy
  layer_add_child(bitmap_layer_get_layer(hourLayer), text_layer_get_layer(minuteLayer));
  layer_add_child(window_layer, bitmap_layer_get_layer(hourLayer));
  layer_add_child(window_layer, bottomBarLayer);
  layer_add_child(window_layer, bitmap_layer_get_layer(batteryLogoLayer));
  layer_add_child(window_layer, text_layer_get_layer(batteryPercentLayer));
  layer_add_child(window_layer, text_layer_get_layer(dateLayer));
  layer_add_child(window_layer, text_layer_get_layer(dayLayer));

  // Avoids a blank screen on watch start.
  time_t tick_time = time(NULL);
  display_time(localtime(&tick_time));
  handle_battery(battery_state_service_peek());
  handle_bluetooth(bluetooth_connection_service_peek());

  tick_timer_service_subscribe(MINUTE_UNIT, &handle_minute_tick);
  battery_state_service_subscribe(&handle_battery);
  bluetooth_connection_service_subscribe(&handle_bluetooth);

  initialized = true;
}

void handle_deinit() {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
  text_layer_destroy(batteryPercentLayer);
  text_layer_destroy(minuteLayer);
  text_layer_destroy(dateLayer);
  text_layer_destroy(dayLayer);
  layer_destroy(bottomBarLayer);

  unload_digit_image();
  bitmap_layer_destroy(hourLayer);
  
  unload_battery_image();
  bitmap_layer_destroy(batteryLogoLayer);
  
  window_destroy(window);
}

int main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}

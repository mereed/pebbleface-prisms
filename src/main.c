/*

Copyright (C) 2015 Mark Reed

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), 
to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

-------------------------------------------------------------------

*/
#include "pebble.h"
#include "effect_layer.h"
	
EffectLayer* effect_layer;
EffectMask mask;

static AppSync sync;
static uint8_t sync_buffer[256];

enum key {
  BLUETOOTHVIBE_KEY = 0x0,
  HOURLYVIBE_KEY = 0x1,
  COLOUR_KEY = 0x2,
  INFO_KEY = 0x3
};

static int bluetoothvibe;
static int hourlyvibe;
static int colour;
static int info;

static bool appStarted = false;

#define TOTAL_DATE_DIGITS 2
#define TOTAL_TIME_DIGITS 4
#define TOTAL_TIMEB_DIGITS 4

static const int DAY_NAME_IMAGE_RESOURCE_IDS[] = {
  RESOURCE_ID_IMAGE_DAY_NAME_SUN,
  RESOURCE_ID_IMAGE_DAY_NAME_MON,
  RESOURCE_ID_IMAGE_DAY_NAME_TUE,
  RESOURCE_ID_IMAGE_DAY_NAME_WED,
  RESOURCE_ID_IMAGE_DAY_NAME_THU,
  RESOURCE_ID_IMAGE_DAY_NAME_FRI,
  RESOURCE_ID_IMAGE_DAY_NAME_SAT
};

#define TOTAL_BATTERY_PERCENT_DIGITS 3
static GBitmap *battery_percent_image[TOTAL_BATTERY_PERCENT_DIGITS];
static BitmapLayer *battery_percent_layers[TOTAL_BATTERY_PERCENT_DIGITS];

const int BATT_IMAGE_RESOURCE_IDS[] = {
  RESOURCE_ID_IMAGE_NUM_B0,
  RESOURCE_ID_IMAGE_NUM_B1,
  RESOURCE_ID_IMAGE_NUM_B2,
  RESOURCE_ID_IMAGE_NUM_B3,
  RESOURCE_ID_IMAGE_NUM_B4,
  RESOURCE_ID_IMAGE_NUM_B5,
  RESOURCE_ID_IMAGE_NUM_B6,
  RESOURCE_ID_IMAGE_NUM_B7,
  RESOURCE_ID_IMAGE_NUM_B8,
  RESOURCE_ID_IMAGE_NUM_B9,
  RESOURCE_ID_IMAGE_TINY_PERCENT
};

static const int BIG_DIGIT_IMAGE_RESOURCE_IDS[] = {
  RESOURCE_ID_IMAGE_NUM_0,
  RESOURCE_ID_IMAGE_NUM_1,
  RESOURCE_ID_IMAGE_NUM_2,
  RESOURCE_ID_IMAGE_NUM_3,
  RESOURCE_ID_IMAGE_NUM_4,
  RESOURCE_ID_IMAGE_NUM_5,
  RESOURCE_ID_IMAGE_NUM_6,
  RESOURCE_ID_IMAGE_NUM_7,
  RESOURCE_ID_IMAGE_NUM_8,
  RESOURCE_ID_IMAGE_NUM_9
};

static Window *s_main_window;
static GBitmap  *s_day_name_bitmap;

#ifdef PBL_PLATFORM_APLITE
static GBitmap *s_white_bitmap, *s_black_bitmap;
#else
static GBitmap *background_image;
#endif

static BitmapLayer *s_background_layer, *s_day_name_layer;

static GBitmap *s_date_digits[TOTAL_DATE_DIGITS];
static GBitmap *s_time_digits[TOTAL_TIME_DIGITS];
static BitmapLayer *s_date_digits_layers[TOTAL_DATE_DIGITS];
static BitmapLayer *s_time_digits_layers[TOTAL_TIME_DIGITS];


static uint8_t batteryPercent;

static GBitmap *battery_image;
static BitmapLayer *battery_image_layer;
static BitmapLayer *battery_layer;

int charge_percent = 0;

BitmapLayer *layer_conn_img;
GBitmap *img_bt_connect;
GBitmap *img_bt_disconnect;
BitmapLayer *bluetooth_layer;
GBitmap *bluetooth_image = NULL;


static void set_container_image(GBitmap **bmp_image, BitmapLayer *bmp_layer, const int resource_id, GPoint origin) {
  GBitmap *old_image = *bmp_image;

  *bmp_image = gbitmap_create_with_resource(resource_id);
	
  GRect bounds = gbitmap_get_bounds(*bmp_image);

  GRect main_frame = GRect(origin.x, origin.y, bounds.size.w, bounds.size.h);
  bitmap_layer_set_bitmap(bmp_layer, *bmp_image);
  layer_set_frame(bitmap_layer_get_layer(bmp_layer), main_frame);

  if (old_image != NULL) {
  	gbitmap_destroy(old_image);
  }
}

void colour_choice() {

	if (mask.bitmap_background) {
		gbitmap_destroy(mask.bitmap_background);
		mask.bitmap_background = NULL;
    }
		
	switch (colour) {
		case 0:
		mask.bitmap_background = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BG);
		break;
		
		case 1:
		mask.bitmap_background = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BG_B);
		break;
			
		case 2:
  		mask.bitmap_background = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BG_C);
		break;
			
		case 3:
		mask.bitmap_background = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BG_D);
		break;
		
		case 4:
		mask.bitmap_background = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BG_E);
		break;
    }
    
	if (mask.bitmap_background != NULL) {
	effect_layer_add_effect(effect_layer, effect_mask, &mask);
	layer_mark_dirty(effect_layer_get_layer(effect_layer));
	} 
}

static void sync_tuple_changed_callback(const uint32_t key,
                                        const Tuple* new_tuple,
                                        const Tuple* old_tuple,
                                        void* context) {

  // App Sync keeps new_tuple in sync_buffer, so we may use it directly
  switch (key) {
	  
    case BLUETOOTHVIBE_KEY:
      bluetoothvibe = new_tuple->value->uint8 != 0;
	  persist_write_bool(BLUETOOTHVIBE_KEY, bluetoothvibe);
      break;      
	  
    case HOURLYVIBE_KEY:
      hourlyvibe = new_tuple->value->uint8 != 0;
	  persist_write_bool(HOURLYVIBE_KEY, hourlyvibe);	  
      break;	    

		case COLOUR_KEY:
      colour = new_tuple->value->uint8;
	  persist_write_bool(COLOUR_KEY, colour);
	  colour_choice();   

	  break;
	  
	case INFO_KEY:
      info = new_tuple->value->uint8 != 0;
	  persist_write_bool(INFO_KEY, info);

	  	if (info) {
				layer_set_hidden(bitmap_layer_get_layer(battery_layer), false);
				layer_set_hidden(bitmap_layer_get_layer(battery_image_layer), false);
				layer_set_hidden(bitmap_layer_get_layer(s_day_name_layer), false);
				layer_set_hidden(bitmap_layer_get_layer(layer_conn_img), false);
				layer_set_hidden(bitmap_layer_get_layer(bluetooth_layer), false);

			    for (int i = 0; i < TOTAL_BATTERY_PERCENT_DIGITS; ++i) {
			      layer_set_hidden(bitmap_layer_get_layer(battery_percent_layers[i]), false);
			    }
			
			    for (int i = 0; i < TOTAL_DATE_DIGITS; ++i) {
			      layer_set_hidden(bitmap_layer_get_layer(s_date_digits_layers[i]), false);
			    } 

		} else {
				layer_set_hidden(bitmap_layer_get_layer(battery_layer), true);
				layer_set_hidden(bitmap_layer_get_layer(battery_image_layer), true);
				layer_set_hidden(bitmap_layer_get_layer(s_day_name_layer), true);
				layer_set_hidden(bitmap_layer_get_layer(layer_conn_img), true);
				layer_set_hidden(bitmap_layer_get_layer(bluetooth_layer), true);

			    for (int i = 0; i < TOTAL_BATTERY_PERCENT_DIGITS; ++i) {
			      layer_set_hidden(bitmap_layer_get_layer(battery_percent_layers[i]), true);
			    }
			
			    for (int i = 0; i < TOTAL_DATE_DIGITS; ++i) {
			      layer_set_hidden(bitmap_layer_get_layer(s_date_digits_layers[i]), true);
			    } 	
	}
		
      break;
	  
  }
}

void update_battery(BatteryChargeState charge_state) {

  batteryPercent = charge_state.charge_percent;

  if(batteryPercent>=98) {
	layer_set_hidden(bitmap_layer_get_layer(battery_image_layer), false);
    for (int i = 0; i < TOTAL_BATTERY_PERCENT_DIGITS; ++i) {
      layer_set_hidden(bitmap_layer_get_layer(battery_percent_layers[i]), true);
    }  
    return;
  }

	  layer_set_hidden(bitmap_layer_get_layer(battery_image_layer), true);

 for (int i = 0; i < TOTAL_BATTERY_PERCENT_DIGITS; ++i) {
    layer_set_hidden(bitmap_layer_get_layer(battery_percent_layers[i]), false);
  }  
	
#ifdef PBL_PLATFORM_CHALK
  set_container_image(&battery_percent_image[0], battery_percent_layers[0], BATT_IMAGE_RESOURCE_IDS[charge_state.charge_percent/10], GPoint(72, 5));
  set_container_image(&battery_percent_image[1], battery_percent_layers[1], BATT_IMAGE_RESOURCE_IDS[charge_state.charge_percent%10], GPoint(84, 5));
  set_container_image(&battery_percent_image[2], battery_percent_layers[2], BATT_IMAGE_RESOURCE_IDS[10], GPoint(96, 5));
#else
  set_container_image(&battery_percent_image[0], battery_percent_layers[0], BATT_IMAGE_RESOURCE_IDS[charge_state.charge_percent/10], GPoint(107, 83));
  set_container_image(&battery_percent_image[1], battery_percent_layers[1], BATT_IMAGE_RESOURCE_IDS[charge_state.charge_percent%10], GPoint(119, 83));
  set_container_image(&battery_percent_image[2], battery_percent_layers[2], BATT_IMAGE_RESOURCE_IDS[10], GPoint(131, 83));
#endif


}

void toggle_bluetooth_icon(bool connected) {
	
    if (connected) {
        bitmap_layer_set_bitmap(layer_conn_img, img_bt_connect);
    } else {
        bitmap_layer_set_bitmap(layer_conn_img, img_bt_disconnect);
    }

    if (appStarted && bluetoothvibe) {
      
        vibes_long_pulse();
	}
}

void bluetooth_connection_callback(bool connected) {
  toggle_bluetooth_icon(connected);

}

void force_update(void) {
  update_battery(battery_state_service_peek());
  bluetooth_connection_callback(bluetooth_connection_service_peek());

}

static unsigned short get_display_hour(unsigned short hour) {
  if (clock_is_24h_style()) {
    return hour;
  }

  unsigned short display_hour = hour % 12;
  return display_hour ? display_hour : 12;
}

static void update_display(struct tm *current_time) {
#ifdef PBL_PLATFORM_CHALK
  set_container_image(&s_day_name_bitmap, s_day_name_layer, DAY_NAME_IMAGE_RESOURCE_IDS[current_time->tm_wday], GPoint(5, 83));	
  set_container_image(&s_date_digits[0], s_date_digits_layers[0], BATT_IMAGE_RESOURCE_IDS[current_time->tm_mday / 10], GPoint(119, 83));
  set_container_image(&s_date_digits[1], s_date_digits_layers[1], BATT_IMAGE_RESOURCE_IDS[current_time->tm_mday % 10], GPoint(131, 83));
#else
  set_container_image(&s_day_name_bitmap, s_day_name_layer, DAY_NAME_IMAGE_RESOURCE_IDS[current_time->tm_wday], GPoint(0, 71));	
  set_container_image(&s_date_digits[0], s_date_digits_layers[0], BATT_IMAGE_RESOURCE_IDS[current_time->tm_mday / 10], GPoint(119, 71));
  set_container_image(&s_date_digits[1], s_date_digits_layers[1], BATT_IMAGE_RESOURCE_IDS[current_time->tm_mday % 10], GPoint(131, 71));
#endif
		

	  unsigned short display_hour = get_display_hour(current_time->tm_hour);
		

#ifdef PBL_PLATFORM_CHALK
  set_container_image(&s_time_digits[0], s_time_digits_layers[0], BIG_DIGIT_IMAGE_RESOURCE_IDS[display_hour / 10], GPoint(35, 23));
  set_container_image(&s_time_digits[1], s_time_digits_layers[1], BIG_DIGIT_IMAGE_RESOURCE_IDS[display_hour % 10], GPoint(97, 23));
#else
  set_container_image(&s_time_digits[0], s_time_digits_layers[0], BIG_DIGIT_IMAGE_RESOURCE_IDS[display_hour / 10], GPoint(12, 12));
  set_container_image(&s_time_digits[1], s_time_digits_layers[1], BIG_DIGIT_IMAGE_RESOURCE_IDS[display_hour % 10], GPoint(84, 12));
#endif
	
#ifdef PBL_PLATFORM_CHALK
  set_container_image(&s_time_digits[2], s_time_digits_layers[2], BIG_DIGIT_IMAGE_RESOURCE_IDS[current_time->tm_min / 10], GPoint(35, 96));
  set_container_image(&s_time_digits[3], s_time_digits_layers[3], BIG_DIGIT_IMAGE_RESOURCE_IDS[current_time->tm_min % 10], GPoint(97, 96));		
#else
  set_container_image(&s_time_digits[2], s_time_digits_layers[2], BIG_DIGIT_IMAGE_RESOURCE_IDS[current_time->tm_min / 10], GPoint(12, 96));
  set_container_image(&s_time_digits[3], s_time_digits_layers[3], BIG_DIGIT_IMAGE_RESOURCE_IDS[current_time->tm_min % 10], GPoint(84, 96));
#endif
	

  if (!clock_is_24h_style()) {

    if (display_hour / 10 == 0) {
    	layer_set_hidden(bitmap_layer_get_layer(s_time_digits_layers[0]), true);
    } else {
    	layer_set_hidden(bitmap_layer_get_layer(s_time_digits_layers[0]), false);
    }
  }
}

static void update_hours(struct tm *tick_time) {

  if(appStarted && hourlyvibe) {
    //vibe!
    vibes_short_pulse();
  }
}

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
	
    update_display(tick_time);
	
  if (units_changed & HOUR_UNIT) {
    update_hours(tick_time);
  }
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);

	
  // Create time and date layers
  GRect dummy_frame = GRect(0, 0, 0, 0);
	
 for (int i = 0; i < TOTAL_TIME_DIGITS; ++i) {
    s_time_digits_layers[i] = bitmap_layer_create(dummy_frame);
    layer_add_child(window_layer, bitmap_layer_get_layer(s_time_digits_layers[i]));
  }
	
  //creating effect layer
  effect_layer = effect_layer_create(GRect(0,0,180,180));
	
  mask.text = NULL;
  mask.bitmap_mask = NULL;
  mask.mask_color = GColorWhite;
  mask.background_color = GColorClear;
  mask.bitmap_background = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BG);
 
  // adding mask effect
  effect_layer_add_effect(effect_layer, effect_mask, &mask);  
  layer_add_child(window_layer, effect_layer_get_layer(effect_layer));


  img_bt_connect     = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BLUETOOTHON);
  img_bt_disconnect  = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BLUETOOTHOFF);

#ifdef PBL_PLATFORM_CHALK
  layer_conn_img  = bitmap_layer_create(GRect(80, 3, 37, 13));
#else
  layer_conn_img  = bitmap_layer_create(GRect(35, 83, 37, 13));
#endif
  bitmap_layer_set_bitmap(layer_conn_img, img_bt_connect);
  layer_add_child(window_layer, bitmap_layer_get_layer(layer_conn_img)); 
	
  bluetooth_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BLUETOOTH);
  GRect bitmap_bounds = gbitmap_get_bounds(bluetooth_image);
  GRect frame = GRect(0, 83, bitmap_bounds.size.w, bitmap_bounds.size.h);
  bluetooth_layer = bitmap_layer_create(frame);
  bitmap_layer_set_bitmap(bluetooth_layer, bluetooth_image);
  layer_add_child(window_layer, bitmap_layer_get_layer(bluetooth_layer));	

  battery_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY);
  GRect bitmap_bounds2 = gbitmap_get_bounds(battery_image);
  GRect frame2 = GRect(90, 83, bitmap_bounds2.size.w, bitmap_bounds2.size.h);
  battery_layer = bitmap_layer_create(frame2);
  battery_image_layer = bitmap_layer_create(frame2);
  bitmap_layer_set_bitmap(battery_image_layer, battery_image);
 
  layer_add_child(window_layer, bitmap_layer_get_layer(battery_image_layer));
  layer_add_child(window_layer, bitmap_layer_get_layer(battery_layer));	

  layer_set_hidden(bitmap_layer_get_layer(battery_layer), true);
  layer_set_hidden(bitmap_layer_get_layer(battery_image_layer), true);
	
  s_day_name_layer = bitmap_layer_create(dummy_frame);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_day_name_layer));

  for (int i = 0; i < TOTAL_DATE_DIGITS; ++i) {
    s_date_digits_layers[i] = bitmap_layer_create(dummy_frame);
    layer_add_child(window_layer, bitmap_layer_get_layer(s_date_digits_layers[i]));
  }
	
  for (int i = 0; i < TOTAL_BATTERY_PERCENT_DIGITS; ++i) {
    battery_percent_layers[i] = bitmap_layer_create(dummy_frame);
    layer_add_child(window_layer, bitmap_layer_get_layer(battery_percent_layers[i]));
  }
}

static void main_window_unload(Window *window) {
	

  layer_remove_from_parent(bitmap_layer_get_layer(s_background_layer));
  bitmap_layer_destroy(s_background_layer);
// gbitmap_destroy(background_image);

  layer_remove_from_parent(bitmap_layer_get_layer(bluetooth_layer));
  bitmap_layer_destroy(bluetooth_layer);
  gbitmap_destroy(bluetooth_image);
	
  layer_remove_from_parent(bitmap_layer_get_layer(layer_conn_img));
  bitmap_layer_destroy(layer_conn_img);
  gbitmap_destroy(img_bt_connect);
  gbitmap_destroy(img_bt_disconnect);

  layer_remove_from_parent(bitmap_layer_get_layer(s_day_name_layer));
  bitmap_layer_destroy(s_day_name_layer);
  gbitmap_destroy(s_day_name_bitmap);

  layer_remove_from_parent(bitmap_layer_get_layer(battery_image_layer));
  bitmap_layer_destroy(battery_image_layer);
	
  layer_remove_from_parent(bitmap_layer_get_layer(battery_layer));
  bitmap_layer_destroy(battery_layer);
  gbitmap_destroy(battery_image);
  battery_image = NULL;
	
  for (int i = 0; i < TOTAL_DATE_DIGITS; i++) {
    layer_remove_from_parent(bitmap_layer_get_layer(s_date_digits_layers[i]));
    gbitmap_destroy(s_date_digits[i]);
    bitmap_layer_destroy(s_date_digits_layers[i]);
  }

  for (int i = 0; i < TOTAL_TIME_DIGITS; i++) {
    layer_remove_from_parent(bitmap_layer_get_layer(s_time_digits_layers[i]));
    gbitmap_destroy(s_time_digits[i]);
    bitmap_layer_destroy(s_time_digits_layers[i]);
	s_time_digits[i] = NULL;
	s_time_digits_layers[i] = NULL;
  }

  for (int i = 0; i < TOTAL_BATTERY_PERCENT_DIGITS; i++) {
    layer_remove_from_parent(bitmap_layer_get_layer(battery_percent_layers[i]));
    gbitmap_destroy(battery_percent_image[i]);
    bitmap_layer_destroy(battery_percent_layers[i]); 
	battery_percent_image[i] = NULL;
	battery_percent_layers[i] = NULL;
  } 
	
  //clearning MASK
  gbitmap_destroy(mask.bitmap_background);
  effect_layer_destroy(effect_layer);
	
}

static void init() {
  memset(&s_time_digits_layers, 0, sizeof(s_time_digits_layers));
  memset(&s_time_digits, 0, sizeof(s_time_digits));

  memset(&s_date_digits_layers, 0, sizeof(s_date_digits_layers));
  memset(&s_date_digits, 0, sizeof(s_date_digits));
	
  memset(&battery_percent_layers, 0, sizeof(battery_percent_layers));
  memset(&battery_percent_image, 0, sizeof(battery_percent_image));

	  // Setup messaging
  const int inbound_size = 256;
  const int outbound_size = 256;
  app_message_open(inbound_size, outbound_size);	
	
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);


  Tuplet initial_values[] = {
    TupletInteger(BLUETOOTHVIBE_KEY, persist_read_bool(BLUETOOTHVIBE_KEY)),
    TupletInteger(HOURLYVIBE_KEY, persist_read_bool(HOURLYVIBE_KEY)),
	TupletInteger(COLOUR_KEY, persist_read_bool(COLOUR_KEY)),
    TupletInteger(INFO_KEY, persist_read_bool(INFO_KEY)),
  };


  app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values,
                ARRAY_LENGTH(initial_values), sync_tuple_changed_callback,
                NULL, NULL);

  appStarted = true;

  // Avoids a blank screen on watch start.
  time_t now = time(NULL);
  struct tm *tick_time = localtime(&now);
  update_display(tick_time);

  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
	
  // handlers
  battery_state_service_subscribe(&update_battery);
  bluetooth_connection_service_subscribe(toggle_bluetooth_icon);

  // draw first frame
  force_update();
}

static void deinit() {
  app_sync_deinit(&sync);
  
  tick_timer_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
  battery_state_service_unsubscribe();
	
  window_destroy(s_main_window);
}

int main() {
  init();
  app_event_loop();
  deinit();
}

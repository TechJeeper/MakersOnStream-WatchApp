#include <pebble.h>
#include "data.h"
#include "comm.h"
#include "ui.h"

static void refresh_later(void *ctx) {
  (void)ctx;
  comm_request_refresh();
}

static void init(void) {
  data_init();
  ui_init();
  comm_init();
  app_timer_register(300, refresh_later, NULL);
}

static void deinit(void) {
  ui_deinit();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

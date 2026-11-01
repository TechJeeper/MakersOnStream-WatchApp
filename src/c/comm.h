#pragma once

#include <pebble.h>

enum {
  CMD_REFRESH = 1,
  CMD_BEGIN = 2,
  CMD_ITEM = 3,
  CMD_DONE = 4,
  CMD_ERROR = 5
};

void comm_init(void);
void comm_request_refresh(void);

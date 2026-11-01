#include "comm.h"
#include "data.h"

extern void ui_reload(void);

static const char *tuple_str(DictionaryIterator *it, uint32_t key) {
  Tuple *t = dict_find(it, key);
  if (!t || t->type != TUPLE_CSTRING) {
    return "";
  }
  return t->value->cstring;
}

static int32_t tuple_int(DictionaryIterator *it, uint32_t key, int32_t fallback) {
  Tuple *t = dict_find(it, key);
  if (!t) return fallback;
  if (t->type == TUPLE_INT) return t->value->int32;
  if (t->type == TUPLE_UINT) return (int32_t)t->value->uint32;
  return fallback;
}

static void inbox_received(DictionaryIterator *it, void *context) {
  (void)context;
  int32_t cmd = tuple_int(it, MESSAGE_KEY_Cmd, 0);

  if (cmd == CMD_BEGIN) {
    data_begin(
      (uint8_t)tuple_int(it, MESSAGE_KEY_LiveCount, 0),
      (uint8_t)tuple_int(it, MESSAGE_KEY_SoonCount, 0),
      (uint8_t)tuple_int(it, MESSAGE_KEY_Mode, 1)
    );
    ui_reload();
    return;
  }

  if (cmd == CMD_ITEM) {
    data_set_item(
      (uint8_t)tuple_int(it, MESSAGE_KEY_Kind, 0),
      (uint8_t)tuple_int(it, MESSAGE_KEY_Index, 0),
      tuple_str(it, MESSAGE_KEY_Name),
      tuple_str(it, MESSAGE_KEY_Subtitle),
      tuple_str(it, MESSAGE_KEY_When),
      tuple_str(it, MESSAGE_KEY_Platform)
    );
    return;
  }

  if (cmd == CMD_DONE) {
    data_done();
    ui_reload();
    return;
  }

  if (cmd == CMD_ERROR) {
    const char *msg = tuple_str(it, MESSAGE_KEY_Status);
    data_error(msg[0] ? msg : "Couldn't load");
    ui_reload();
  }
}

static void outbox_failed(DictionaryIterator *it, AppMessageResult reason, void *context) {
  (void)it;
  (void)reason;
  (void)context;
  data_error("Phone offline");
  ui_reload();
}

void comm_request_refresh(void) {
  data_set_loading("Refreshing…");
  ui_reload();

  DictionaryIterator *it;
  AppMessageResult r = app_message_outbox_begin(&it);
  if (r != APP_MSG_OK) {
    data_error("Can't reach phone");
    ui_reload();
    return;
  }
  dict_write_int32(it, MESSAGE_KEY_Cmd, CMD_REFRESH);
  app_message_outbox_send();
}

void comm_init(void) {
  app_message_register_inbox_received(inbox_received);
  app_message_register_outbox_failed(outbox_failed);
  app_message_open(256, 128);
}

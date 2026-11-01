#include "data.h"

static MosData s_data;

static void copy_field(char *dst, size_t dst_len, const char *src) {
  if (!src) {
    dst[0] = '\0';
    return;
  }
  strncpy(dst, src, dst_len - 1);
  dst[dst_len - 1] = '\0';
}

void data_init(void) {
  memset(&s_data, 0, sizeof(s_data));
  s_data.state = MOS_STATE_LOADING;
  copy_field(s_data.status, sizeof(s_data.status), "Loading…");
}

MosData *data_get(void) {
  return &s_data;
}

void data_begin(uint8_t live_count, uint8_t soon_count, uint8_t mode) {
  if (live_count > MOS_MAX_LIVE) live_count = MOS_MAX_LIVE;
  if (soon_count > MOS_MAX_SOON) soon_count = MOS_MAX_SOON;
  memset(s_data.live, 0, sizeof(s_data.live));
  memset(s_data.soon, 0, sizeof(s_data.soon));
  s_data.live_count = live_count;
  s_data.soon_count = soon_count;
  s_data.mode = mode;
  s_data.state = MOS_STATE_LOADING;
  copy_field(s_data.status, sizeof(s_data.status), "Updating…");
}

void data_set_item(uint8_t kind, uint8_t index, const char *name,
                   const char *subtitle, const char *when, const char *platform) {
  MosItem *item = NULL;
  if (kind == MOS_KIND_LIVE && index < MOS_MAX_LIVE) {
    item = &s_data.live[index];
    item->kind = MOS_KIND_LIVE;
  } else if (kind == MOS_KIND_SOON && index < MOS_MAX_SOON) {
    item = &s_data.soon[index];
    item->kind = MOS_KIND_SOON;
  }
  if (!item) return;
  copy_field(item->name, sizeof(item->name), name);
  copy_field(item->subtitle, sizeof(item->subtitle), subtitle);
  copy_field(item->when, sizeof(item->when), when);
  copy_field(item->platform, sizeof(item->platform), platform);
}

void data_done(void) {
  s_data.state = MOS_STATE_READY;
  if (s_data.live_count == 0 && s_data.soon_count == 0) {
    copy_field(s_data.status, sizeof(s_data.status), "Nothing scheduled");
  } else {
    s_data.status[0] = '\0';
  }
}

void data_error(const char *msg) {
  s_data.state = MOS_STATE_ERROR;
  s_data.live_count = 0;
  s_data.soon_count = 0;
  copy_field(s_data.status, sizeof(s_data.status), msg ? msg : "Error");
}

void data_set_loading(const char *msg) {
  s_data.state = MOS_STATE_LOADING;
  copy_field(s_data.status, sizeof(s_data.status), msg ? msg : "Loading…");
}

const MosItem *data_item_at(uint8_t section, uint8_t row) {
  if (section == 0 && row < s_data.live_count) {
    return &s_data.live[row];
  }
  if (section == 1 && row < s_data.soon_count) {
    return &s_data.soon[row];
  }
  return NULL;
}

uint8_t data_section_count(void) {
  return 2;
}

uint8_t data_rows_in_section(uint8_t section) {
  if (s_data.state != MOS_STATE_READY) {
    return section == 0 ? 1 : 0;
  }
  if (section == 0) {
    return s_data.live_count > 0 ? s_data.live_count : 1;
  }
  if (section == 1) {
    return s_data.soon_count > 0 ? s_data.soon_count : 1;
  }
  return 0;
}

#pragma once

#include <pebble.h>

#define MOS_NAME_LEN 28
#define MOS_SUB_LEN 40
#define MOS_WHEN_LEN 24
#define MOS_PLAT_LEN 12
#define MOS_STATUS_LEN 40
#define MOS_MAX_LIVE 8
#define MOS_MAX_SOON 16

typedef enum {
  MOS_KIND_LIVE = 0,
  MOS_KIND_SOON = 1
} MosKind;

typedef struct {
  char name[MOS_NAME_LEN];
  char subtitle[MOS_SUB_LEN];
  char when[MOS_WHEN_LEN];
  char platform[MOS_PLAT_LEN];
  MosKind kind;
} MosItem;

typedef enum {
  MOS_STATE_LOADING = 0,
  MOS_STATE_READY,
  MOS_STATE_ERROR
} MosState;

typedef struct {
  MosState state;
  uint8_t mode; // 0 following, 1 all
  uint8_t live_count;
  uint8_t soon_count;
  MosItem live[MOS_MAX_LIVE];
  MosItem soon[MOS_MAX_SOON];
  char status[MOS_STATUS_LEN];
} MosData;

void data_init(void);
MosData *data_get(void);
void data_begin(uint8_t live_count, uint8_t soon_count, uint8_t mode);
void data_set_item(uint8_t kind, uint8_t index, const char *name,
                   const char *subtitle, const char *when, const char *platform);
void data_done(void);
void data_error(const char *msg);
void data_set_loading(const char *msg);
const MosItem *data_item_at(uint8_t section, uint8_t row);
uint8_t data_section_count(void);
uint8_t data_rows_in_section(uint8_t section);

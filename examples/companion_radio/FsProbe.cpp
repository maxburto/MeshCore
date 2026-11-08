#ifdef FS_PROBE
#include <Arduino.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include <string.h>
#include "FsProbe.h"
extern "C" {
#include "littlefs/lfs.h"
}

struct Region { const char* name; uint32_t addr; uint32_t size; };
static const Region INTERNAL_FS = {"internal", 0xED000, 7 * 4096};
static const Region EXTRA_FS = {"extra", 0xD4000, 0x19000};
static const uint32_t BLOCK = 128;

static int err_internal = 1, err_extra = 1;
static uint32_t reset_reason;
static uint8_t file_id, file_prefs, file_bond;

// The flash is memory mapped, so a read needs no driver and cannot touch what it is looking at.
static int ro_read(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, void* buffer, lfs_size_t size) {
  memcpy(buffer, (const void*)((uint32_t)c->context + block * BLOCK + off), size);
  return 0;
}
static int ro_prog(const struct lfs_config*, lfs_block_t, lfs_off_t, const void*, lfs_size_t) { return LFS_ERR_IO; }
static int ro_erase(const struct lfs_config*, lfs_block_t) { return LFS_ERR_IO; }
static int ro_sync(const struct lfs_config*) { return 0; }

static int tryMount(const Region& r) {
  static lfs_t lfs;
  struct lfs_config cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.context = (void*)r.addr;
  cfg.read = ro_read; cfg.prog = ro_prog; cfg.erase = ro_erase; cfg.sync = ro_sync;
  cfg.read_size = BLOCK; cfg.prog_size = BLOCK; cfg.block_size = BLOCK;
  cfg.block_count = r.size / BLOCK;
  cfg.lookahead = 128;
  int err = lfs_mount(&lfs, &cfg);
  if (err == 0) lfs_unmount(&lfs);
  return err;
}

static void pageSummary(const Region& r) {
  for (uint32_t page = 0; page < r.size / 4096; page++) {
    uint32_t base = r.addr + page * 4096;
    int erased = 0;
    for (uint32_t b = 0; b < 4096 / BLOCK; b++) {
      const uint8_t* p = (const uint8_t*)(base + b * BLOCK);
      bool ff = true;
      for (uint32_t i = 0; i < BLOCK && ff; i++) ff = (p[i] == 0xFF);
      if (ff) erased++;
    }
    Serial.printf("PROBE page %s %05lX erased_blocks=%d/32 first=%02X%02X%02X%02X\n", r.name, base, erased,
                  ((const uint8_t*)base)[0], ((const uint8_t*)base)[1], ((const uint8_t*)base)[2], ((const uint8_t*)base)[3]);
    delay(5);
  }
}

static void hexDump(const Region& r) {
  for (uint32_t b = 0; b < r.size / BLOCK; b++) {
    const uint8_t* p = (const uint8_t*)(r.addr + b * BLOCK);
    Serial.printf("HEX %s %05lX ", r.name, (uint32_t)p);
    for (uint32_t i = 0; i < BLOCK; i++) Serial.printf("%02X", p[i]);
    Serial.println();
    delay(2);
  }
  Serial.printf("HEXEND %s\n", r.name);
}

static void report() {
  Serial.printf("PROBE mount internal=%d extra=%d resetreas=0x%08lX id=%d prefs=%d bond=%d up=%lu\n", err_internal, err_extra,
                reset_reason, file_id, file_prefs, file_bond, millis());
}

void fsProbeBeforeMount() {
  reset_reason = NRF_POWER->RESETREAS;
  NRF_POWER->RESETREAS = reset_reason;
  err_internal = tryMount(INTERNAL_FS);
  err_extra = tryMount(EXTRA_FS);
  if (err_internal == 0 && err_extra == 0) return;

  // Stop here: going on would format and destroy the evidence. A 1200-baud touch still enters the bootloader.
  // 'c' carries on (and the framework then formats), 'd' and 'e' dump the regions again.
  bool dumped = false;
  for (;;) {
    if (Serial) {
      report();
      pageSummary(INTERNAL_FS);
      pageSummary(EXTRA_FS);
      if (!dumped) { hexDump(INTERNAL_FS); dumped = true; }
    }
    for (int i = 0; i < 100; i++) {
      delay(100);
      int ch = Serial.read();
      if (ch == 'd') hexDump(INTERNAL_FS);
      if (ch == 'e') hexDump(EXTRA_FS);
      if (ch == 'c') { Serial.println("PROBE carrying on"); return; }
    }
  }
}

void fsProbeAfterStore() {
  file_id = InternalFS.exists("/_main.id");
  file_prefs = InternalFS.exists("/new_prefs");
  file_bond = InternalFS.exists("/adafruit/bond_prph");
}

void fsProbeTick() {
  static uint32_t next;
  if ((int32_t)(millis() - next) < 0) return;
  next = millis() + 10000;
  if (Serial) report();
}

void fsProbeOversize(float snr, float rssi, const unsigned char* raw, int len) {
  if (!Serial) return;
  Serial.printf("OVERSIZE len=%d snr=%.2f rssi=%.0f up=%lu raw=", len, snr, rssi, millis());
  for (int i = 0; i < len; i++) Serial.printf("%02X", raw[i]);
  Serial.println();
}
#endif

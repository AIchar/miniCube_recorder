#ifndef ARZHE_SDCARD_H
#define ARZHE_SDCARD_H

#ifdef __cplusplus
extern "C" {
#endif


#include <string.h>
#include "esp_system.h"


#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"

#include "driver/sdmmc_host.h"

#define MOUNT_POINT "/sdcard"
#define MAX_OPEN_FILE   5

typedef unsigned int uint_t;

#define GPIO_SD_CLK     14
#define GPIO_SD_CMD     15
#define GPIO_SD_D0      2
#define GPIO_SD_D1      4
#define GPIO_SD_D2      12
#define GPIO_SD_D3      13

uint8_t arzhe_sdcard_init();
void arzhe_sdcard_unmount();
void arzhe_print_card_info();
void arzhe_sdcard_get_list(char *dir_path);
unsigned char arzhe_sdcard_new(FIL *fp, char* path);
uint8_t arzhe_sdcard_nwav(FIL *fp, char* path, uint32_t wavLen);
unsigned char arzhe_sdcard_write(FIL *fp, const void* buf, uint_t byte2Write);
unsigned char arzhe_sdcard_read(char* path, void* buf,unsigned int byte2Read);
unsigned char arzhe_sdcard_write_p(FIL *fp, const void* buf, uint_t byte2Write, unsigned int pos);
unsigned int arzhe_sdcard_get_file_size(FIL *fp);
unsigned char arzhe_sdcard_close(FIL *fp);

#ifdef __cplusplus
}
#endif
#endif
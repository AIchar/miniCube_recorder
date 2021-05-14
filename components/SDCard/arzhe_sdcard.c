#include "arzhe_sdcard.h"

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

#include "arzhe_wave.h"

static const char *TAG = "arzhe_sdcard";

static sdmmc_card_t* card;
static uint8_t g_flag_mounted = false;

void arzhe_print_card_info(){
    
    if (g_flag_mounted == false)
    {
        ESP_LOGE(TAG, "Please mount sdcard first!!!");
        return;
    }
    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);
}

void arzhe_sdcard_get_list(char *dir_path){
  
    if (g_flag_mounted == false)
    {
        ESP_LOGE(TAG, "Please mount sdcard first!!!");
        return;
    }
    
    char sDirPath[128] = {0};
  
    FF_DIR dp;
    FRESULT ret;
    FILINFO fileInfo;
    int numOfFile = 0;
    int numOfDir = 0;
    if (dir_path != NULL)
    {
        sprintf(sDirPath, "/%s", dir_path);
    } else {
        sprintf(sDirPath, "/");
        // sprintf(sDirPath, "/%s", MOUNT_POINT);
    }
    
    ret = f_opendir(&dp, sDirPath);
    if (ret == FR_NO_PATH)
    {
        ESP_LOGE(TAG, "DIR %s dose no exist", sDirPath);
        return;
    } else if (ret != FR_OK)
    {
        ESP_LOGE(TAG, "FAIL TO OPEN %s", sDirPath);
        return;
    }

    while(1){
        ret = f_readdir(&dp, &fileInfo);
        if (ret != FR_OK || fileInfo.fname[0]==0)
        {
            ESP_LOGI(TAG, "DIR NULL!!!");
            break;
        }else if (fileInfo.fattrib & AM_DIR) //文件夹
        {
            ESP_LOGI(TAG,"D %s", fileInfo.fname);
            numOfDir++;
            continue;
        }else {
            ESP_LOGI(TAG,"F %s", fileInfo.fname);
            numOfFile++;
            continue;
        }        
    }
    ESP_LOGI(TAG, "FILE:%d Dir:%d", numOfFile, numOfDir);
    f_closedir(&dp);
    
    
}

uint8_t arzhe_sdcard_init(){
    
    esp_err_t ret;
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = MAX_OPEN_FILE,
        .allocation_unit_size = 16 * 1024
    };
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGD(TAG, "Mountting SD card");

    // sdmmc_card_t* card;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    gpio_set_pull_mode(GPIO_SD_CMD, GPIO_PULLUP_ONLY);   // CMD, needed in 4- and 1- line modes
    gpio_set_pull_mode(GPIO_SD_D0,  GPIO_PULLUP_ONLY);    // D0, needed in 4- and 1-line modes
    gpio_set_pull_mode(GPIO_SD_D1,  GPIO_PULLUP_ONLY);    // D1, needed in 4-line mode only
    gpio_set_pull_mode(GPIO_SD_D2,  GPIO_PULLUP_ONLY);   // D2, needed in 4-line mode only
    gpio_set_pull_mode(GPIO_SD_D3,  GPIO_PULLUP_ONLY);   // D3, needed in 4- and 1-line modes

    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                "If you want the card to be formatted, set the format_if_mount_failed to true.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return 0;
    }
    ESP_LOGI(TAG, "Mount card success!!");
    g_flag_mounted = true;
    return 1;
}

void arzhe_sdcard_unmount(){
    if (g_flag_mounted)
    {
        const char mount_point[] = MOUNT_POINT;
        esp_vfs_fat_sdcard_unmount(mount_point, card);
        ESP_LOGI(TAG, "Card unmounted");
        g_flag_mounted = false;
    } else {
        ESP_LOGE(TAG, "SDCARD IS NOT MOUNTED");
    }
    
}

uint8_t arzhe_sdcard_ismount(){
    return g_flag_mounted;
}

uint8_t arzhe_sdcard_new(FIL *fp, char* path){
    FRESULT ret;
    ret = f_open(fp, path, FA_CREATE_NEW | FA_WRITE);
    switch (ret)
    {
    case FR_OK:
        ESP_LOGD(TAG, "Create %s success!!", path);
        return 1;
        break;
    case FR_NO_PATH:
        ESP_LOGE(TAG, "invalid path %s", path);
        break;
    case FR_INVALID_NAME:
        ESP_LOGE(TAG, "invalid name");
        break;
    case FR_DENIED:
        ESP_LOGE(TAG, "FR_DENIED");
        break;
    case FR_WRITE_PROTECTED:
        ESP_LOGE(TAG, "FR_WRITE_PROTECTED");
        break;   
    default:
        ESP_LOGE(TAG, "UNKNOWN error :%d", ret);
        break;
    }
    return 0;
}


uint8_t arzhe_sdcard_nwav(FIL *fp, char* path, uint32_t wavLen){
    
    FRESULT ret;
    Wav wav_header = {
        .riff={
            .ChunkID = {'R','I','F','F'},
            .ChunkSize = (sizeof(Wav)+wavLen-8),
            .Format = {'W','A','V','E'},
        },
        .fmt.Subchunk1ID = {'f','m','t',' '},
        .fmt.Subchunk1Size = 16,
        .fmt.AudioFormat = 1,
        .fmt.NumChannels = 1,
        .fmt.SampleRate = 16000,
        .fmt.ByteRate = 32000,
        .fmt.BlockAlign = 2,
        .fmt.BitsPerSample = 16,
        .data.Subchunk2ID = {'d','a','t','a'},
        .data.Subchunk2Size = wavLen,
    };

    ESP_LOGI(TAG, "openning %s", path);
    ret = f_open(fp, path, FA_CREATE_NEW | FA_WRITE);
    switch (ret)
    {
    case FR_OK:
        ESP_LOGD(TAG, "Create %s success!!", path);
        arzhe_sdcard_write(fp,(void*)&wav_header, sizeof(wav_header));
        return 1;
        break;
    case FR_NO_PATH:
        ESP_LOGE(TAG, "invalid path %s", path);
        break;
    case FR_INVALID_NAME:
        ESP_LOGE(TAG, "invalid name");
        break;
    case FR_DENIED:
        ESP_LOGE(TAG, "FR_DENIED");
        break;
    case FR_WRITE_PROTECTED:
        ESP_LOGE(TAG, "FR_WRITE_PROTECTED");
        break;  
    case FR_EXIST:
        ESP_LOGI(TAG, "file is exist");
        return 2;
        break; 
    default:
        ESP_LOGE(TAG, "UNKNOWN error :%d", ret);
        break;
    }
    return 0;
}


uint8_t arzhe_sdcard_write(FIL *fp, const void* buf, uint_t byte2Write){
    uint_t byteWrite = 0;
    if(f_write(fp, buf, byte2Write, &byteWrite) == FR_OK){
        if (byteWrite == byte2Write)
        {
            ESP_LOGI(TAG, "SUCCESS WRITE %d Bytes", byteWrite);
            f_sync(fp);
            return 1;
        }
    }
    ESP_LOGE(TAG, "error WRITE %d Bytes", byteWrite);
    return 0;
}

uint8_t arzhe_sdcard_read(char* path, void* buf,uint32_t byte2Read){
    uint32_t byteRead = 0;
    FRESULT ret;
    static FIL fp;
    ret = f_open(&fp, path, FA_READ);
    switch (ret)
    {
    case FR_OK:
        ESP_LOGD(TAG, "Open  %s success!!", path);
        if(f_read(&fp, buf, byte2Read, &byteRead) == FR_OK){
            ESP_LOGI(TAG, "READ %d bytes", byteRead);
            f_close(&fp);
            return 1;
        }
        f_close(&fp);
        break;
    case FR_NO_PATH:
        ESP_LOGE(TAG, "invalid path %s", path);
        break;
    case FR_INVALID_NAME:
        ESP_LOGE(TAG, "invalid name");
        break;
    case FR_DENIED:
        ESP_LOGE(TAG, "FR_DENIED");
        break;
    case FR_WRITE_PROTECTED:
        ESP_LOGE(TAG, "FR_WRITE_PROTECTED");
        break;   
    default:
        ESP_LOGE(TAG, "UNKNOWN error :%d", ret);
        break;
    }
    return 0;

}

uint8_t arzhe_sdcard_write_p(FIL *fp, const void* buf, uint_t byte2Write, uint32_t pos){
    
    uint_t byteWrite = 0;
    if(f_lseek(fp, pos) != FR_OK) return 0;
    if(f_write(fp, buf, byte2Write, &byteWrite) == FR_OK){
        if (byteWrite == byte2Write)
        {
            f_sync(fp);
            return 1;
        }
    }
    return 0;
}

uint32_t arzhe_sdcard_get_file_size(FIL *fp){
    if (fp != NULL)
    {
        return f_size(fp);
    }
    return 0;
}

uint8_t arzhe_sdcard_close(FIL *fp){
    f_close(fp);
    return 0;
}
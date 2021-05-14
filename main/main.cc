/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

// #include "I2SSampler.h"
#include "miniCube_conifg.h"
#include "button.h"
#include "I2SMEMSSampler.h"
#include "arzhe_sdcard.h"
#include "arzhe_wave.h"

/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi station";

static int s_retry_num = 0;
I2SSampler *i2sSampler = NULL;
uint8_t flag_recoder = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

extern "C" void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config;
    //  = {
    //     .sta = {
    //         .ssid = EXAMPLE_ESP_WIFI_SSID,
    //         .password = EXAMPLE_ESP_WIFI_PASS,
    //         /* Setting a password implies station will connect to all security modes including WEP/WPA.
    //          * However these modes are deprecated and not advisable to be used. Incase your Access point
    //          * doesn't support WPA2, these mode can be enabled by commenting below line */
	//     //  .threshold.authmode = WIFI_AUTH_WPA2_PSK,

    //         .pmf_cfg = {
    //             .capable = true,
    //             .required = false
    //         },
    //     },
    // };
    memcpy(wifi_config.sta.ssid, "doit-1", strlen("doit-1"));
    memcpy(wifi_config.sta.password, "doit3305", strlen("doit3305"));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}


// Task to write samples to our server
FIL file;
void i2sMemsWriterTask(void *param)
{
  I2SSampler *sampler = (I2SSampler *)param;
  const TickType_t xMaxBlockTime = pdMS_TO_TICKS(100);
//   int16_t *audioBuffer = NULL;
    int recoder_num = 0;
  while (true)
  {
    // wait for some samples to save
    uint32_t ulNotificationValue = ulTaskNotifyTake(pdTRUE, xMaxBlockTime);
    if (ulNotificationValue > 0 && flag_recoder == 1)
    {
        // audioBuffer = sampler->getCapturedAudioBuffer();
    //   sendData(wifiClientI2S, httpClientI2S, I2S_SERVER_URL, (uint8_t *)sampler->getCapturedAudioBuffer(), sampler->getBufferSizeInBytes());
        if (recoder_num==0)
        {
            ESP_LOGI(TAG, "RECODEDING!!!!");
            recoder_num+=1;
            continue;
        }else{
            // arzhe_sdcard_write(&file, sampler->getCapturedAudioBuffer(), sampler->getBufferSizeInBytes());
            // ESP_LOGI(TAG, "RECODE SUCCESS");
            // arzhe_sdcard_close(&file);
            // ESP_LOGI(TAG, "RECODE END");
            // recoder_num=0;
            // flag_recoder = 0;
        }  
    }
  }
}

void task_new_recoder(void *param){

    char path[64] = {0};
    int16_t writeBuf[1600];
    static int index = 0;
    int last_audio_pos = i2sSampler->getCurrWritePos();
    RingBufferAccessor *reader = i2sSampler->getRingBufferReader();
    reader->setIndex(last_audio_pos);
    reader->rewind(16000);
    uint8_t ret;
    do
    {
        memset(path, 0, sizeof(path));
        sprintf(path,"/dw%04d.wav", index++);
        ret = arzhe_sdcard_nwav(&file, path, 16000*2*1);
        if (ret == 1)
        {
            flag_recoder = 1;
            for (int i = 0; i < 10; i++)
            {
                for (int j = 0; j < 1600; j++)
                {
                    writeBuf[j] = reader->getCurrentSample();
                    reader->moveToNextSample();
                }
                arzhe_sdcard_write(&file, writeBuf, 1600*2);
            }
            arzhe_sdcard_close(&file);
            ESP_LOGI(TAG, "RECODER END");
            break;
        }
    } while (ret == 2);
    flag_recoder=0;
    vTaskDelete(NULL);
}

void button_power_short_press(){
    ESP_LOGI(TAG, "BUTTON POWER SHORT PRESS!!");
    if(flag_recoder == 1){
        return;
    }
    xTaskCreate(task_new_recoder, "new recoder Task", 1024 * 8, NULL, 1, NULL);
}

void button_power_long_press(){
    ESP_LOGI(TAG, "BUTTON POWER LONG PRESS!!");
    if (arzhe_sdcard_ismount())
    {
        /* code */
        arzhe_sdcard_unmount();
    } else { 
        arzhe_sdcard_init();
    }
    
}


extern "C" void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // i2s config for reading from both channels of I2S
     


    // // wifi_init_sta();


    if(arzhe_sdcard_init() == 1){
        ESP_LOGI(TAG, "CARD MOUNT SUCCESS");
        arzhe_print_card_info();
        arzhe_sdcard_get_list(NULL);
        // arzhe_sdcard_unmount();
    }


    // Direct i2s input from INMP441 or the SPH0645
    i2sSampler = new I2SMEMSSampler(GPIO_MIC_SCK, GPIO_MIC_SD, GPIO_MIC_WS);

    // set up the i2s sample writer task
    TaskHandle_t i2sMemsWriterTaskHandle;
    xTaskCreatePinnedToCore(i2sMemsWriterTask, "I2S Writer Task", 4096, i2sSampler, 1, &i2sMemsWriterTaskHandle, 1);

    // // start sampling from i2s device
    i2sSampler->start(I2S_NUM_1, i2sMemsWriterTaskHandle);
    // // vTaskDelay(10000 / portTICK_PERIOD_MS);
    // // esp_restart();
    button_press_init((gpio_num_t)BUTTOM_POWER, (button_cb)button_power_short_press, (button_cb)button_power_long_press);

}

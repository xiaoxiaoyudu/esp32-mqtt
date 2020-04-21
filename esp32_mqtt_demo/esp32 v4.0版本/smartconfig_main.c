/* Esptouch example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "tcpip_adapter.h"
#include "esp_smartconfig.h"

#include "nvs_flash.h"
#include "mqtt_client.h"
#include "driver/gpio.h"//gpio头文件
#define LED GPIO_NUM_2
#include "cJSON.h"

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t s_wifi_event_group;//wifi事件组
static EventGroupHandle_t mqtt_event_group;//mqtt事件组
/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;
static const int WIFI_LED = BIT2;

static const char *TAG = "smartconfig_example";
esp_mqtt_client_handle_t client;
static void smartconfig_example_task(void * parm);
void wifi_init(void);
void led_init(void);
void led_show(void);
void wifi_connet(void);
void ctrl_led(char *text)
{
    cJSON *root;
    //截取有效json
    char *index=strchr(text,'{');
    strcpy(text,index);

    root = cJSON_Parse(text); 
    if(root!=NULL)
    {
        cJSON *c = cJSON_GetObjectItem(root,"id");
       	printf("\n****led%s****\n",c->valuestring);
	if(!strcmp("on",(const char *)c->valuestring))
	{
         gpio_set_level(LED, 1);//开
	}
    else
    {
        gpio_set_level(LED, 0);//关
    }
    

    }
    //ESP_LOGI(HTTP_TAG,"%s 222",__func__);
    cJSON_Delete(root);
}
/******下面是mqtt回调函数******/
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

         msg_id = esp_mqtt_client_publish(client, "sw_led", "hello", 0, 1, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, "sw_led", 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);



            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);

            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            ctrl_led(event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .host = "ip地址",
        .event_handle = mqtt_event_handler,
        .username="用户名",
        .password="密码",
        .client_id="设备id",
        .keepalive=60
    };
#if CONFIG_BROKER_URL_FROM_STDIN
    char line[128];

    if (strcmp(mqtt_cfg.uri, "FROM_STDIN") == 0) {
        int count = 0;
        printf("Please enter url of mqtt broker\n");
        while (count < 128) {
            int c = fgetc(stdin);
            if (c == '\n') {
                line[count] = '\0';
                break;
            } else if (c > 0 && c < 127) {
                line[count] = c;
                ++count;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        mqtt_cfg.uri = line;
        printf("Broker url: %s\n", line);
    } else {
        ESP_LOGE(TAG, "Configuration mismatch: wrong broker url");
        abort();
    }
#endif /* CONFIG_BROKER_URL_FROM_STDIN */

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}


/**********下面是wifi网络********/

static void event_handler(void* arg, esp_event_base_t event_base, 
                                int32_t event_id, void* event_data)//网络回调函数
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {//开始配网
          
           
       
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);//清除连接标志
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {//获取ip

        xEventGroupSetBits(s_wifi_event_group, WIFI_LED);//设置连接标志状态
        xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);//设置连接标志状态
         mqtt_app_start();
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG, "Scan done");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
        ESP_LOGI(TAG, "Found channel");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {//获取密码
        ESP_LOGI(TAG, "Got SSID and password");

        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        wifi_config_t wifi_config;
        uint8_t ssid[33] = { 0 };
        uint8_t password[65] = { 0 };

        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
        wifi_config.sta.bssid_set = evt->bssid_set;
        if (wifi_config.sta.bssid_set == true) {
            memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
        }

        memcpy(ssid, evt->ssid, sizeof(evt->ssid));
        memcpy(password, evt->password, sizeof(evt->password));
        ESP_LOGI(TAG, "SSID:%s", ssid);
        ESP_LOGI(TAG, "PASSWORD:%s", password);

        /*打开wifipass工作区并保存密码*/
            nvs_handle my_handle;//nvs句柄
            esp_err_t err = nvs_open("wifi_info", NVS_READWRITE, &my_handle);//打开wifipass空间
            err = nvs_set_str(my_handle,"wifi_ssid",(char *)ssid);
            err = nvs_set_str(my_handle,"wifi_pass",(char *)password);
            err =nvs_commit(my_handle);
           
            uint8_t i;
            if(err==ESP_OK)
            {   
                printf( "save ok\n");
            }
            else
            {
                printf("save fail\n");
            }
            
            nvs_close(my_handle);
        ESP_ERROR_CHECK( esp_wifi_disconnect() );
        ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
        ESP_ERROR_CHECK( esp_wifi_connect() );
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
        xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
    }
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();//tcp初始化
   
    ESP_ERROR_CHECK(esp_event_loop_create_default());//创建循环事件

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();//定义配置文件
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );//配置文件初始化

    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );//任何一个时间都回调
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );//STA模式
    ESP_ERROR_CHECK( esp_wifi_start() );//wifi启动
     wifi_connet();
}

static void smartconfig_example_task(void * parm)
{

    EventBits_t uxBits;
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_AIRKISS) );//选择esptouch和airkiss配网
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_smartconfig_start(&cfg) );
    while (1) {
        uxBits = xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY); //等待配网事件组
        if(uxBits & CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi Connected to ap");
        }
        if(uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "smartconfig over");
            esp_smartconfig_stop();
            vTaskDelete(NULL);
        }
    }
}


void app_main()
{
    ESP_ERROR_CHECK( nvs_flash_init() );//检查nvs_flash初始化
    led_init();
    wifi_init();
    
}
void led_init()
{
    gpio_pad_select_gpio( LED);
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);
}
void led_show(void)
{
    while(1)
    {
      if(xEventGroupGetBits(s_wifi_event_group)&WIFI_LED)
     {
        gpio_set_level(LED, 0);
        break;
      }
      else
      {
        gpio_set_level(LED, 0);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        /* Blink on (output high) */
        gpio_set_level(LED, 1);
        vTaskDelay(500 / portTICK_PERIOD_MS);
      }
      

    }
     vTaskDelete(NULL);
}
void wifi_init(void)
{
    s_wifi_event_group = xEventGroupCreate();
      initialise_wifi();//配网
      xTaskCreate(led_show, "led_show", 1000, NULL, 1, NULL);
}
void wifi_connet(void)
{
        size_t ssid_len =40;
        size_t pass_len =20;
        int8_t i;
        uint8_t ssid[33] = { 0 };
        uint8_t password[65] = { 0 };
        /*打开wifipass工作区*/
        nvs_handle my_handle;//nvs句柄
        esp_err_t err = nvs_open("wifi_info", NVS_READWRITE, &my_handle);//打开wifipass空间
        
       /*从nvs中获取到ssid和密码*/
        err =nvs_get_str(my_handle,"wifi_ssid",(char *)ssid,&ssid_len);
        if(err==ESP_OK)
        {
           i=strlen((char *)ssid);
             printf( "get wifissid %d:",i);
            printf("%s\r\n",ssid);
        }       
        err =nvs_get_str(my_handle,"wifi_pass",(char *)password,&pass_len);
        if(err==ESP_OK)
        {
            i=strlen((char *)password);
             printf("get wifipass %d:",i);
            printf("%s\r\n",password);

        }         
         nvs_close(my_handle); 
        EventBits_t uxBits;
    wifi_config_t wifi_config ;

         bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

        for(i=0;i<2;i++)//自动连接两次，连接阻塞时间5秒
        {
           /*连接wifi*/
        printf("connet  %d\r\n",i);
        ESP_ERROR_CHECK( esp_wifi_disconnect() );
        ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
        ESP_ERROR_CHECK( esp_wifi_connect() );
        /*进入阻塞态等待连接*/
        uxBits = xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT, true, false, 20000/portTICK_PERIOD_MS); 
         if(uxBits & CONNECTED_BIT) {
            printf("WiFi Connected to ap\n");
         return;

        }
        }
        printf("smartconfig\n");
     xTaskCreate(smartconfig_example_task, "smartconfig_example_task", 4096, NULL, 3, NULL);
       
}

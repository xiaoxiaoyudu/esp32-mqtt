/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "string.h"
#include "driver/uart.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event_loop.h"
#include "tcpip_adapter.h"
#include "esp_smartconfig.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "cJSON.h"
#include "mqtt_client.h"
#define rxbuf 1024//接受容量大小
#define txbuf 512
#define tx GPIO_NUM_12//发送引脚
#define rx GPIO_NUM_13//接受引脚
#define led 2
#define key 4
#define UART_NUM1 02
/*天气预报*/
#define false		0
#define true		1
#define errno		(*__errno())

//http组包宏，获取天气的http接口参数
#define WEB_SERVER          "api.thinkpage.cn"              
#define WEB_PORT            "80"
#define WEB_URL             "/v3/weather/now.json?key="
#define hosturl		        "api.thinkpage.cn"
#define APIKEY		        "g3egns3yk2ahzb0p"       
#define city		        "suzhou"
#define language	        "en"
/*
===========================
全局变量定义
=========================== 
*/
//http请求包
static const char *REQUEST = "GET "WEB_URL""APIKEY"&location="city"&language="language" HTTP/1.1\r\n"
    "Host: "WEB_SERVER"\r\n"
    "Connection: close\r\n"
    "\r\n";

//wifi链接成功事件
static EventGroupHandle_t wifi_event_group;
static EventGroupHandle_t mqtt_event_group;
//天气解析结构体
typedef struct 
{
    char cit[20];
    char weather_text[20];
    char weather_code[2];
    char temperatur[3];
}weather_info;

weather_info weathe;

unsigned char ledon=0;//led灯状态

static EventGroupHandle_t s_wifi_event_group;//指向函数的指针

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;
/*日志*/
static const char *TAG = "sc";

static const char *HTTP_TAG = "http_task";

char wifi_ssid=0;

/*主要函数*/
void smartconfig_example_task(void * parm);
void wifi_init(void);
void uart_init(void);
void nvs_init(void);
void wifi_connet(void * parm);
void lcdshow(char *c);
void led_init(void);
void led_show(void * parm);
void http_get_task(void *pvParameters);
/*解析json数据 只处理 解析 城市 天气 天气代码  温度  其他的自行扩展
* @param[in]   text  		       :json字符串
* @retval      void                 :无
* @note        修改日志 
*               Ver0.0.1:
                    hx-zsj, 2018/08/10, 初始化版本\n 
*/
/*mqtt发送信息*/
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED://mqtt连接成功
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            msg_id = esp_mqtt_client_publish(client, "sw_led", "connet", 0, 1, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            xEventGroupSetBits(mqtt_event_group, CONNECTED_BIT);
            msg_id = esp_mqtt_client_subscribe(client, "sw_led", 1);
            break;
        case MQTT_EVENT_DISCONNECTED://mqtt连接失败
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            xEventGroupClearBits(mqtt_event_group, CONNECTED_BIT);
            break;

        case MQTT_EVENT_SUBSCRIBED://mqtt订阅
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
          
            
            break;
        case MQTT_EVENT_UNSUBSCRIBED://mqtt取消订阅
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED://mqtt发送
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA://mqtt接受数据
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("%c",event->data[0]);
            if (event->data[0]=='n')
            {
                      gpio_set_level(key, 0);
                printf("ok");
            }
            if (event->data[0]=='f')
            {
                        printf("no");
                      gpio_set_level(key, 1);

            }
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR://错误事件
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            xEventGroupClearBits(mqtt_event_group, CONNECTED_BIT);
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}
static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .host = "ip",
        .event_handle = mqtt_event_handler,
        .username="用户名",
        .password="密码",
        .client_id="设备id",
        .keepalive=60

        // .user_context = (void *)your_context
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

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);
    xEventGroupWaitBits(mqtt_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
}
/*json解析*/
void cjson_to_struct_info(char *text)
{
    cJSON *root,*psub;
    cJSON *arrayItem;
    //截取有效json
    char *index=strchr(text,'{');
    strcpy(text,index);

    root = cJSON_Parse(text);
    
    if(root!=NULL)
    {
        psub = cJSON_GetObjectItem(root, "results");
        arrayItem = cJSON_GetArrayItem(psub,0);

        cJSON *locat = cJSON_GetObjectItem(arrayItem, "location");
        cJSON *now = cJSON_GetObjectItem(arrayItem, "now");
        if((locat!=NULL)&&(now!=NULL))
        {
            psub=cJSON_GetObjectItem(locat,"name");
            sprintf(weathe.cit,"%s",psub->valuestring);
            ESP_LOGI(HTTP_TAG,"city:%s",weathe.cit);

            psub=cJSON_GetObjectItem(now,"text");
            sprintf(weathe.weather_text,"%s",psub->valuestring);
            ESP_LOGI(HTTP_TAG,"weather:%s",weathe.weather_text);
            
            psub=cJSON_GetObjectItem(now,"code");
            sprintf(weathe.weather_code,"%s",psub->valuestring);
            //ESP_LOGI(HTTP_TAG,"%s",weathe.weather_code);

            psub=cJSON_GetObjectItem(now,"temperature");
            sprintf(weathe.temperatur,"%s",psub->valuestring);
            ESP_LOGI(HTTP_TAG,"temperatur:%s",weathe.temperatur);

            //ESP_LOGI(HTTP_TAG,"--->city %s,weather %s,temperature %s<---\r\n",weathe.cit,weathe.weather_text,weathe.temperatur);
        }
    }
    //ESP_LOGI(HTTP_TAG,"%s 222",__func__);
    cJSON_Delete(root);
}
/*http请求*/
void http_get_task(void *pvParameters)
{
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    struct in_addr *addr;
    int s, r;
    char recv_buf[1024];
    char mid_buf[1024];
    
    while(1) {
        
        //DNS域名解析
        int err = getaddrinfo(WEB_SERVER, WEB_PORT, &hints, &res);
        if(err != 0 || res == NULL) {
            ESP_LOGE(HTTP_TAG, "DNS lookup failed err=%d res=%p\r\n", err, res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        //打印获取的IP
        addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
        //ESP_LOGI(HTTP_TAG, "DNS lookup succeeded. IP=%s\r\n", inet_ntoa(*addr));

        //新建socket
        s = socket(res->ai_family, res->ai_socktype, 0);
        if(s < 0) {
            ESP_LOGE(HTTP_TAG, "... Failed to allocate socket.\r\n");
            close(s);
            freeaddrinfo(res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        //连接ip
        if(connect(s, res->ai_addr, res->ai_addrlen) != 0) {
            ESP_LOGE(HTTP_TAG, "... socket connect failed errno=%d\r\n", errno);
            close(s);
            freeaddrinfo(res);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        freeaddrinfo(res);
        //发送http包
        if (write(s, REQUEST, strlen(REQUEST)) < 0) {
            ESP_LOGE(HTTP_TAG, "... socket send failed\r\n");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        //清缓存
        memset(mid_buf,0,sizeof(mid_buf));
        //获取http应答包
        do {
            bzero(recv_buf, sizeof(recv_buf));
            r = read(s, recv_buf, sizeof(recv_buf)-1);
            strcat(mid_buf,recv_buf);
        } while(r > 0);
        //json解析
        cjson_to_struct_info(mid_buf);
        //关闭socket，http是短连接
        close(s);

        //延时一会
        for(int countdown = 100; countdown >= 0; countdown--) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
}

void lcdshow(char *c)
{
    uart_write_bytes(UART_NUM1,c,strlen(c));

}
static esp_err_t event_handler(void *ctx, system_event_t *event)//回调函数
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START://作为sta开始工作
        if(wifi_ssid==0)
        xTaskCreate(smartconfig_example_task, "smartconfig_example_task", 4096, NULL, 3, NULL);
        else
        xTaskCreate(wifi_connet, "wifi_connet", 4096, NULL, 3, NULL);
        
        break;
    case SYSTEM_EVENT_STA_GOT_IP://获得ip
        xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
        
        wifi_ssid=2;//获取密码
          mqtt_app_start();
         xTaskCreate(http_get_task, "http_get_task", 4096, NULL, 3, NULL);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED://断开连接
        esp_wifi_connect();
        wifi_ssid=3;//断开连接
        xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();//初始化tcp/ip配置量
    mqtt_event_group = xEventGroupCreate();
    s_wifi_event_group = xEventGroupCreate();//创建一个时间标志组
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );//初始化事件调度器

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();//wifi配置结构体

    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );//wifi初始化
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );//sta模式
    ESP_ERROR_CHECK( esp_wifi_start() );//开始运行
}

static void sc_callback(smartconfig_status_t status, void *pdata)
{
        int i;
    switch (status) {
        case SC_STATUS_WAIT:
            ESP_LOGI(TAG, "SC_STATUS_WAIT");
            break;
        case SC_STATUS_FIND_CHANNEL:
            ESP_LOGI(TAG, "SC_STATUS_FINDING_CHANNEL");
            break;
        case SC_STATUS_GETTING_SSID_PSWD:
            ESP_LOGI(TAG, "SC_STATUS_GETTING_SSID_PSWD");
            break;
        case SC_STATUS_LINK:
            ESP_LOGI(TAG, "SC_STATUS_LINK");
            wifi_config_t *wifi_config = pdata;
            ESP_LOGI(TAG, "SSID:%s", wifi_config->sta.ssid);
            ESP_LOGI(TAG, "PASSWORD:%s", wifi_config->sta.password);
          
            /*打开wifipass工作区并保存密码*/
            nvs_handle my_handle;//nvs句柄
            char *ssid = (char *)wifi_config->sta.ssid;
           char *word = (char *)wifi_config->sta.password;
            esp_err_t err = nvs_open("wifipass", NVS_READWRITE, &my_handle);//打开wifipass空间
            err = nvs_set_str(my_handle,"wifi_ssid",ssid);
            err = nvs_set_str(my_handle,"wifi_pass",word);
            err =nvs_commit(my_handle);

            if(err==ESP_OK)
            {   
                i=strlen(ssid);

            printf("%s%d\n",ssid,i);
            i=strlen(word);
            printf("%s%d\n",word,i);
                 printf( "save ok\n");
            }
            else
            {
                printf("save fail\n");
            }
            
            nvs_close(my_handle);



            ESP_ERROR_CHECK( esp_wifi_disconnect() );
            ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config) );
            ESP_ERROR_CHECK( esp_wifi_connect() );
            break;
        case SC_STATUS_LINK_OVER:
            ESP_LOGI(TAG, "SC_STATUS_LINK_OVER");
            if (pdata != NULL) {
                uint8_t phone_ip[4] = { 0 };
                memcpy(phone_ip, (uint8_t* )pdata, 4);
                ESP_LOGI(TAG, "Phone ip: %d.%d.%d.%d\n", phone_ip[0], phone_ip[1], phone_ip[2], phone_ip[3]);
            }

            xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);//wifi配置完成

                
            break;
        default:
            break;
    }
}

void smartconfig_example_task(void * parm)
{
    EventBits_t uxBits;
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
    ESP_ERROR_CHECK( esp_smartconfig_start(sc_callback) );
    while (1) {
        uxBits = xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY); 
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


void key_if(void)
{
    
    if(gpio_get_level(key)==0)
    {
    while(gpio_get_level(key)==0);
    if(ledon==0)
    {
        ledon=1;
        gpio_set_level(led,1);


    }
    else{
        ledon=0;
        gpio_set_level(led,0);


    }



    }





}
void uart1_rx_task()
{
    uint8_t *data=(uint8_t*)malloc(rxbuf+1);
    while(1)
    {
        const int re=uart_read_bytes(UART_NUM1,data,rxbuf,10/portTICK_RATE_MS);
        if(re>0)
        {
            data[re]=0;
            printf("%s",data);

        }


    }
    free(data);


}

void app_main()
{ 

    nvs_init();
    led_init();
    xTaskCreate(led_show, "led_show", 1000, NULL, 1, NULL);
    uart_init();
    wifi_init();
    

    //xTaskCreate(uart1_rx_task,"uart_rx_task",1024*2,NULL,configMAX_PRIORITIES,NULL);
    //uart_write_bytes(UART_NUM1,"send ok",strlen("send ok"));
     

   
}
void wifi_init()
{
        nvs_handle my_handle;//nvs句柄
        esp_err_t err = nvs_open("wifipass", NVS_READWRITE, &my_handle);//打开wifipass空间
    if (err != ESP_OK) {//打开失败，进行一键佩网
         printf("no wifipass,smartconfig now\r\n");
         wifi_ssid=0;//表示没有密码


       
    } else {//打开成功
        wifi_ssid=1;//表示有密码
        printf("read wifipass");//从nvc中读取wifi信息


                  } 
                  nvs_close(my_handle);

      initialise_wifi();//配网




}
void uart_init(void)
{
    uart_config_t u1config;
    u1config.baud_rate=9600;
    u1config.data_bits=UART_DATA_8_BITS;
    u1config.parity=UART_PARITY_DISABLE;
    u1config.stop_bits=UART_STOP_BITS_1;
    u1config.flow_ctrl=UART_HW_FLOWCTRL_DISABLE;
    uart_param_config(UART_NUM1,&u1config);
    uart_set_pin(UART_NUM1,tx,rx,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM1,rxbuf*2,txbuf*2,0,NULL,0);
}
void nvs_init(void)
{
    esp_err_t err = nvs_flash_init();//flash初始化
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());//擦除，初始化
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

   printf("nvs init ok\n");//nvs初始化完成

}
void wifi_connet(void * parm)
{

        size_t ssid_len =20;
        size_t pass_len =20;
        int8_t i;
        
        /*打开wifipass工作区*/
        nvs_handle my_handle;//nvs句柄
        esp_err_t err = nvs_open("wifipass", NVS_READWRITE, &my_handle);//打开wifipass空间
        //char ssid2[20];
        //char word[20];
        
       /*从nvs中获取到ssid和密码*/
        //1
        wifi_config_t wifi_config;

        
        err =nvs_get_str(my_handle,"wifi_ssid",(char *)wifi_config.sta.ssid,&ssid_len);
        if(err==ESP_OK)
        {
           i=strlen((char *)wifi_config.sta.ssid);
             printf( "get wifissid %d:",i);
            printf("%s\r\n",wifi_config.sta.ssid);
        }
        
        err =nvs_get_str(my_handle,"wifi_pass",(char *)wifi_config.sta.password,&pass_len);
        if(err==ESP_OK)
        {
            i=strlen((char *)wifi_config.sta.password);
             printf("get wifipass %d:",i);
            printf("%s\r\n",wifi_config.sta.password);

        }
         
         nvs_close(my_handle);

         
      /*   i=0;
         while(1)
        {
            if(i==20||ssid2[i]==0)
                break;
            wifi_config.sta.ssid[i]=(uint8_t)ssid2[i];

        }
         i=0;
         while(1)
        {
            if(i==20||word[i]==0)
                break;
            wifi_config.sta.password[i]=(uint8_t)word[i];

        }
        */
        EventBits_t uxBits;
        for(i=0;i<2;i++)//自动连接两次，连接阻塞时间5秒
        {
            /*连接wifi*/
        printf("connet  %d\r\n",i);
        ESP_ERROR_CHECK( esp_wifi_disconnect() );
        ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
        ESP_ERROR_CHECK( esp_wifi_connect() );
        /*进入阻塞态等待连接*/
        uxBits = xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT, true, false, 5000/portTICK_PERIOD_MS); 
         if(uxBits & CONNECTED_BIT) {
            printf("WiFi Connected to ap\n");
            vTaskDelete(NULL);

        }
        }
        wifi_ssid=0;
        printf("smartconfig\n");
     xTaskCreate(smartconfig_example_task, "smartconfig_example_task", 4096, NULL, 3, NULL);
        vTaskDelete(NULL);
    
   




}
void led_init()
{
    gpio_pad_select_gpio(led);
    gpio_set_direction(led, GPIO_MODE_OUTPUT);
     gpio_pad_select_gpio(key);
    gpio_set_direction(key, GPIO_MODE_OUTPUT);


}
void led_show(void * parm)
{
    while(1)
    {
    switch (wifi_ssid)
    {
    case 0:
         gpio_set_level(led, 0);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        /* Blink on (output high) */
        gpio_set_level(led, 1);
        vTaskDelay(500 / portTICK_PERIOD_MS);


        /* code */
        break;
    case 1:
         gpio_set_level(led, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        /* Blink on (output high) */
        gpio_set_level(led, 1);
        vTaskDelay(500 / portTICK_PERIOD_MS);

        /* code */
        break;
    case 2:

       
        gpio_set_level(led, 1);
        vTaskDelay(3000 / portTICK_PERIOD_MS);

        /* code */
        break;
    case 3:
         gpio_set_level(led, 0);
        vTaskDelay(1500 / portTICK_PERIOD_MS);
        /* Blink on (output high) */
        gpio_set_level(led, 1);
        vTaskDelay(1500 / portTICK_PERIOD_MS);


        /* code */
        break;


    
    default:
        break;
    }
    }

}

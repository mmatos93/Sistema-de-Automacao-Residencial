#include <stdio.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "esp_system.h"
#include "driver/gpio.h"

#include "owb.h"
#include "owb_rmt.h"
#include "ds18b20.h"

#include <esp_event.h>
#include <esp_system.h>
#include <esp_https_server.h>

#define GPIO_DS18B20_0 (14) // gpio conectado no barramento de dados
#define DS18B20_RESOLUTION (DS18B20_RESOLUTION_12_BIT)
#define SAMPLE_PERIOD (1000) // tempo de amostragem em milisegundos

#define LED 27
#define INPUT_SENSOR_PIR 33

//#define SSID "brisa-2726344"
//#define PASS "y6dt3uzn"

#define SSID "SN-WiFi 2.4Ghz"
#define PASS "brl#2017#"

static const char *TAG = "HTTPS";
//Informações do sensor
char movimentoon[30] = "Movimento detectado";
char movimentoff[30] = "Sem movimento";
char statuson[10] = "ligado";
char statusoff[10] = "desligado";

// Define client certificate
extern const uint8_t ClientCert_pem_start[] asm("_binary_ClientCert_pem_start");
extern const uint8_t ClientCert_pem_end[]   asm("_binary_ClientCert_pem_end");

// Define server certificates
extern const unsigned char ServerCert_pem_start[] asm("_binary_ServerCert_pem_start");
extern const unsigned char ServerCert_pem_end[] asm("_binary_ServerCert_pem_end");
extern const unsigned char ServerKey_pem_start[] asm("_binary_ServerKey_pem_start");
extern const unsigned char ServerKey_pem_end[] asm("_binary_ServerKey_pem_end");

// WiFi
static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        printf("WiFi connecting ... \n");
        break;
    case WIFI_EVENT_STA_CONNECTED:
        printf("WiFi connected ... \n");
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        printf("WiFi lost connection ... \n");
        break;
    case IP_EVENT_STA_GOT_IP:
        printf("WiFi got IP ... \n\n");
        break;
    default:
        break;
    }
}

void wifi_connection()
{
    // 1 - Wi-Fi/LwIP Init Phase
    esp_netif_init();                    // TCP/IP initiation 					s1.1
    esp_event_loop_create_default();     // event loop 			                s1.2
    esp_netif_create_default_wifi_sta(); // WiFi station 	                    s1.3
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_initiation); // 					                    s1.4
    // 2 - Wi-Fi Configuration Phase
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = SSID,
            .password = PASS}};
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
    // 3 - Wi-Fi Start Phase
    esp_wifi_start();
    // 4- Wi-Fi Connect Phase
    esp_wifi_connect();
}

// Server
static esp_err_t server_get_handler(httpd_req_t *req)
{
    const char resp[] = "Server GET Response .................";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t server_post_handler(httpd_req_t *req)
{
    char content[100];
    size_t recv_size = MIN(req->content_len, sizeof(content));
    int ret = httpd_req_recv(req, content, recv_size);

    // If no data is send the error will be:
    // W (88470) httpd_uri: httpd_uri: uri handler execution failed
    printf("\nServer POST content: %s\n", content);

    if (ret <= 0)
    { /* 0 return value indicates connection closed */
        /* Check if timeout occurred */
        if (ret == HTTPD_SOCK_ERR_TIMEOUT)
        {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    /* Send a simple response */
    const char resp[] = "Server POST Response .................";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

static const httpd_uri_t server_uri_get = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = server_get_handler
};

static const httpd_uri_t server_uri_post = {
    .uri = "/",
    .method = HTTP_POST,
    .handler = server_post_handler
};

static httpd_handle_t start_webserver(void)
{
    // Start the httpd server
    ESP_LOGI(TAG, "Starting server");
    
    httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();
    httpd_handle_t server = NULL;
    
    config.cacert_pem = ServerCert_pem_start;
    config.cacert_len = ServerCert_pem_end - ServerCert_pem_start;
    config.prvtkey_pem = ServerKey_pem_start;
    config.prvtkey_len = ServerKey_pem_end - ServerKey_pem_start;

    esp_err_t ret = httpd_ssl_start(&server, &config);
    if (ESP_OK != ret)
    {
        ESP_LOGI(TAG, "Error starting server!");
        return NULL;
    }

    // Set URI handlers
    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_register_uri_handler(server, &server_uri_get);
    httpd_register_uri_handler(server, &server_uri_post);
    return server;
}

// Client
esp_err_t client_event_get_handler(esp_http_client_event_handle_t evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        printf("Client HTTP_EVENT_ON_DATA: %.*s\n", evt->data_len, (char *)evt->data);
        break;

    default:
        break;
    }
    return ESP_OK;
}

//FORMATAR JSON
char buff[200];
char * formataRequestJson(float temperatura, char *sensor, char *statusAr) {

  char tempChar[16];
  sprintf(tempChar, "%.3f", temperatura); //Transforma temperatura

  snprintf(buff, sizeof(buff),
           "{\"temperatura\":\"%s\",\"sensor\":\"%s\",\"statusAr\":\"%s\"}",
           tempChar, sensor, statusAr);
    return buff;
  //printf("%s\n", buff);
}

static void client_post_rest_function(float temperatura, char *sensor, char *statusAr)
{
    esp_http_client_config_t config_post = {
        .url = "https://esp3213.herokuapp.com/esp/inserir-informacoes?t=temperatura&s=status&m=movimento",
        .method = HTTP_METHOD_POST,
        .cert_pem = (const char *)ClientCert_pem_start,
        .event_handler = client_event_get_handler};
        printf("chegou aqui 1");
    esp_http_client_handle_t client = esp_http_client_init(&config_post);


    //char  *post_data = "{\"temperatura\":\"21\",\"sensor\":\"presenca detectada\",\"statusAr\":\"ligado\"}";
    char  *post_data = formataRequestJson(temperatura, sensor, statusAr);


    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_http_client_set_header(client, "Content-Type", "application/json");
    printf("chegou aqui 2");
    esp_http_client_perform(client);
    printf("chegou aqui 3");
    esp_http_client_cleanup(client);
}

void app_main(void)
{
    nvs_flash_init();
    wifi_connection();

    vTaskDelay(2000 / portTICK_PERIOD_MS);
    printf("WIFI was initiated ...........\n\n");

    printf("Start server:\n\n");
    start_webserver();

    vTaskDelay(2000 / portTICK_PERIOD_MS);
    printf("Start client:\n\n");

    //client_post_rest_function();
    

    esp_log_level_set("*", ESP_LOG_INFO);
    vTaskDelay(2000.0 / portTICK_PERIOD_MS);

    // Criando o barramento de 1 fio(1-wire bus)
    OneWireBus *owb;
    owb_rmt_driver_info rmt_driver_info;
    owb = owb_rmt_initialize(&rmt_driver_info, GPIO_DS18B20_0, RMT_CHANNEL_1, RMT_CHANNEL_0);
    owb_use_crc(owb, true);

    // Encontrando o dispositivo
    //OneWireBus_ROMCode device_rom_code = {0};
    int num_devices = 0;
    OneWireBus_SearchState search_state = {0};
    bool found = false;
    owb_search_first(owb, &search_state, &found);
    if (found)
    {
        char rom_code_s[17];
        owb_string_from_rom_code(search_state.rom_code, rom_code_s, sizeof(rom_code_s));
        //device_rom_code = search_state.rom_code;
        num_devices = 1;
        printf("Sensor encontrado: %s\n", rom_code_s);
    }
    else
    {
        printf("Sensor de temperatura nao encontrado\n");
    }

    // Criando o dispositivo no barramento
    DS18B20_Info *device = {0};

    DS18B20_Info *ds18b20_info = ds18b20_malloc(); // alocação de memoria para o dispositivo
    device = ds18b20_info;

    ds18b20_init_solo(ds18b20_info, owb); // iniciando um unico dispoitivo

    ds18b20_use_crc(ds18b20_info, true);
    ds18b20_set_resolution(ds18b20_info, DS18B20_RESOLUTION);

    gpio_pad_select_gpio(LED);// Mapeamento do pino LED
    gpio_pad_select_gpio(INPUT_SENSOR_PIR);// Mapeamento do pino PIR
    gpio_set_direction(LED, GPIO_MODE_OUTPUT); // Definição
    gpio_set_direction(INPUT_SENSOR_PIR, GPIO_MODE_INPUT); // Definição pino do sensor pir

    // verifica se o dispositivo está em modo de corrente parasita(alimentado pelo barramento de dados)
    bool parasitic_power = false;
    ds18b20_check_for_parasite_power(owb, &parasitic_power);
    owb_use_parasitic_power(owb, parasitic_power);

    //printf("INFORMAÇÕES DO SENSOR PIR\n");// Mostra informação no terminal

    bool estado_ini = 0 ; // estado inicial do led

    gpio_set_level(LED,estado_ini);
           
	int level = 0;

    int temperaturaBase = 26;

    // variáveis para verificação de erro de leitura e numero da amostra de temperatura atual
    int errors_count = {0};
    int sample_count = 0;
    if (num_devices > 0)
    {
        TickType_t last_wake_time = xTaskGetTickCount();

        while (1)
        {
            // inicia as conversões no dispositivo e espera um tempo pre determinado para finalizar
            ds18b20_convert_all(owb);
            ds18b20_wait_for_conversion(device);

            float readings = {0};
            DS18B20_ERROR errors = {0};

            errors = ds18b20_read_temp(device, &readings);

            printf("\nLeitura de temperatura em graus Celsius | Numero da amostra: %d\n", ++sample_count);

            vTaskDelay(pdMS_TO_TICKS(10));    
            if(gpio_get_level(INPUT_SENSOR_PIR)){
                printf("MOVIMENTO DETECTADO\n\n");
                client_post_rest_function(readings, statusoff, movimentoon);
                if(readings>temperaturaBase){
                    gpio_set_level(LED,0); // Ligar Ar
                    printf("LIGANDO AR CONDICIONADO\n\n");
                    client_post_rest_function(readings, statuson, movimentoon);
                    vTaskDelay(pdMS_TO_TICKS(1000)); 
                }    
            }else{
                printf("SEM MOVIMENTO\n");
                    gpio_set_level(LED,1); // Desligar AR
                    client_post_rest_function(readings, statusoff, movimentoff);
            }
            //client_post_rest_function();
            if (errors != DS18B20_OK)
            {
                ++errors_count;
            }

            printf("  %d: %.1f    %d erros\n", 0, readings, errors_count);

            vTaskDelayUntil(&last_wake_time, SAMPLE_PERIOD / portTICK_PERIOD_MS);
        }
    }
    else
    {
        printf("\nNenhum DS18B20 foi detectado!\n");
    }

    // limpando alocação dinamica

    ds18b20_free(&device);
    owb_uninitialize(owb);
    fflush(stdout);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart();




}

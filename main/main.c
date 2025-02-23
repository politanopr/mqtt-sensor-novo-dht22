#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "cJSON.h"  
#include "dht.h"   //  Biblioteca do DHT22 
#include "sdkconfig.h"  // Inclui as configurações definidas no menuconfig




//  Configurações Wi-Fi

#define WIFI_SSID CONFIG_WIFI_SSID
#define WIFI_PASS CONFIG_WIFI_PASS
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
static EventGroupHandle_t wifi_evento_grupo;
static int num_tentativas = 0;
#define MAX_RETRY 5

//  Configurações MQTT
#define MQTT_BROKER   "mqtt://broker.emqx.io"
#define MQTT_PUBLISH_TOPIC "esp32/test"
#define MQTT_SUBSCRIBE_TOPIC "esp32/command"
#define MQTT_SENSOR_TOPIC "esp32/sensor"  //  Novo tópico para os dados do DHT22

//  Configurações do DHT22
#define DHT_PIN GPIO_NUM_4  //  Altere para o pino correto do seu ESP32

static const char *TAG = "ESP32_MQTT";
static esp_mqtt_client_handle_t cliente;

//  Manipulador de eventos Wi-Fi
static void wifi_evento_manipulador(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "Tentando conectar ao Wi-Fi...");
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                if (num_tentativas < MAX_RETRY) {
                    ESP_LOGW(TAG, "Conexão falhou! Tentando novamente... (%d/%d)", num_tentativas + 1, MAX_RETRY);
                    esp_wifi_connect();
                    num_tentativas++;
                } else {
                    ESP_LOGE(TAG, "Falha ao conectar ao Wi-Fi após %d tentativas",num_tentativas);
                    xEventGroupSetBits(wifi_evento_grupo, WIFI_FAIL_BIT);
                }
                break;
            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "Conectado! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        num_tentativas = 0;
        xEventGroupSetBits(wifi_evento_grupo, WIFI_CONNECTED_BIT);
    }
}

//  Inicializa a conexão Wi-Fi
static void wifi_init(void) {
    wifi_evento_grupo = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_evento_manipulador, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_evento_manipulador, NULL));

    wifi_config_t wifi_configura = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_configura));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi inicializado!");

    //  Aguarda conexão Wi-Fi antes de continuar
    EventBits_t bits = xEventGroupWaitBits(wifi_evento_grupo, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Wi-Fi conectado!");
    } else {
        ESP_LOGE(TAG, "Falha ao conectar ao Wi-Fi!");
    }
}

//  Função para ler e enviar dados do DHT22 via MQTT
void dht22_task(void *pvParameter) {
    while (1) {
        int16_t temperature = 0;
        int16_t humidity = 0;

        if (dht_read_data(DHT_TYPE_AM2301, DHT_PIN, &humidity, &temperature) == ESP_OK) {
            float temp_celsius = temperature / 10.0;
            float hum_percent = humidity / 10.0;
            ESP_LOGI(TAG, "🌡️ Temperatura: %.1f°C  💧 Umidade: %.1f%%", temp_celsius, hum_percent);

            //  Cria o JSON
            cJSON *json = cJSON_CreateObject();
            cJSON_AddNumberToObject(json, "temperatura", temp_celsius);
            cJSON_AddNumberToObject(json, "umidade", hum_percent);
            char *json_string = cJSON_PrintUnformatted(json);

            //  Envia para o broker MQTT
            esp_mqtt_client_publish(cliente, MQTT_SENSOR_TOPIC, json_string, 0, 1, 0);
            ESP_LOGI(TAG, "📤 Dados enviados para o broker: %s", json_string);

            //  Libera memória
            cJSON_Delete(json);
            free(json_string);
        } else {
            ESP_LOGE(TAG, "Falha ao ler o DHT22!");
        }

        vTaskDelay(pdMS_TO_TICKS(60000));  //  Aguarda 60 segundos antes da próxima leitura
    }
}

//  Manipulador de eventos MQTT
static void mqtt_evento_manipulador(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    cliente= event->client;

    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, " Conectado ao broker MQTT!");
            esp_mqtt_client_subscribe(cliente, MQTT_SUBSCRIBE_TOPIC, 1);
            esp_mqtt_client_publish(cliente, MQTT_PUBLISH_TOPIC, "ESP32 conectado!", 0, 1, 0);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Desconectado do broker MQTT!");
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "Mensagem publicada com sucesso!");
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "Mensagem recebida! Tópico: %.*s", event->topic_len, event->topic);
            ESP_LOGI(TAG, "Payload recebido: %.*s", event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, " Erro no MQTT!");
            break;
        default:
            break;
    }
}

// Função principal do MQTT
static void mqtt_app_start(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER,
    };

    cliente = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(cliente, ESP_EVENT_ANY_ID, mqtt_evento_manipulador, cliente);
    esp_mqtt_client_start(cliente);

    xTaskCreate(&dht22_task, "dht22_task", 4096, NULL, 5, NULL);  //  Inicia a leitura do DHT22
}

//  Função principal (app_main)
void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init();
    mqtt_app_start();
}

#include "mqtt_utils.h"

#include "globals.h"
#include "os.h"

#include <esp_event.h>
#include <esp_netif.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <lwip/sockets.h>
#include <nvs_flash.h>

bool is_wifi_connected = false;


static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	if (event_base == WIFI_EVENT) {
		switch (event_id) {
			case WIFI_EVENT_STA_START: {
				esp_wifi_connect();
			} break;
			case WIFI_EVENT_STA_CONNECTED: {
				ESP_LOGI(TAG, "connected to AP");
			} break;
			case WIFI_EVENT_STA_DISCONNECTED: {
				esp_wifi_connect();
				ESP_LOGI(TAG, "connect to the AP fail,retry now");
			} break;
			default: break;
		}
	}
	if (event_base == IP_EVENT) {
		switch (event_id) {
			case IP_EVENT_STA_GOT_IP: {
				ESP_LOGI(TAG, "get ip address");
				is_wifi_connected = true;
			} break;
			default: break;
		}
	}
}


void connect_to_wifi(void) {
	ESP_ERROR_CHECK(nvs_flash_init());
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

	wifi_config_t wifi_cfg = {
		.sta =
			{
				.ssid = WIFI_SSID,
				.password = WIFI_PWD,
				.threshold.authmode = WIFI_MODE,
				.pmf_cfg =
					{
						.capable = true,
						.required = false,
					},
			},
	};

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_cfg));
	ESP_ERROR_CHECK(esp_wifi_start());

	while (!is_wifi_connected) {
		delay(ms_to_ticks(20000));
		ESP_LOGI(TAG, "Connecting to WiFi...");
	}
}


const char connection_search[] = "search";
const char connection_found[] = "found";

bool find_mqtt_ip(char* ip) {
	// broadcast to port 1884
	struct sockaddr_in broadcast_addr;
	broadcast_addr.sin_family = AF_INET;
	broadcast_addr.sin_port = htons(1884);
	broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;

	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		ESP_LOGE(TAG, "Failed to create socket");
		return false;
	}

	int broadcast = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
		ESP_LOGE(TAG, "Failed to set socket option");
		closesocket(sock);
		return false;
	}

	if (sendto(sock, connection_search, sizeof(connection_search), 0, (struct sockaddr*)&broadcast_addr,
			   sizeof(broadcast_addr))
		< 0) {
		ESP_LOGE(TAG, "Failed to send broadcast");
		closesocket(sock);
		return false;
	}

	struct sockaddr_in from;
	socklen_t fromlen = sizeof(from);
	char buf[64];
	int len = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromlen);
	if (len < 0) {
		ESP_LOGE(TAG, "Failed to receive broadcast");
		closesocket(sock);
		return false;
	}

	if (strncmp(buf, connection_found, sizeof(connection_found)) == 0) {
		strcpy(ip, inet_ntoa(from.sin_addr));
		ESP_LOGI(TAG, "Found MQTT broker at %s", ip);
		closesocket(sock);
		return true;
	} else {
		ESP_LOGE(TAG, "Invalid response from MQTT broker");
		closesocket(sock);
		return false;
	}
}

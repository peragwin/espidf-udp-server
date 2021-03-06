/* BSD Socket API Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#define PORT CONFIG_EXAMPLE_PORT

static const char *TAG = "example";
static int sock = -1;
static bool sock_bound = false;

static void udp_server_task(void *pvParameters)
{
    char rx_buffer[1460];
    char addr_str[128];
    int addr_family = AF_INET;
    int core = (int)pvParameters;
    int ip_protocol = 0;
    struct sockaddr_in6 dest_addr;

    uint32_t packet_count = 0;
    uint32_t byte_count = 0;
    int last_report = 0;
    ESP_LOGI(TAG, "udp_task core %d", core);

    while (1) {

        if (core == 1) {
            if (addr_family == AF_INET) {
                struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
                dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
                dest_addr_ip4->sin_family = AF_INET;
                dest_addr_ip4->sin_port = htons(PORT);
                ip_protocol = IPPROTO_IP;
            } else if (addr_family == AF_INET6) {
                bzero(&dest_addr.sin6_addr.un, sizeof(dest_addr.sin6_addr.un));
                dest_addr.sin6_family = AF_INET6;
                dest_addr.sin6_port = htons(PORT);
                ip_protocol = IPPROTO_IPV6;
            }

            sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
            if (sock < 0) {
                ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
                break;
            }
            ESP_LOGI(TAG, "Socket created");
            int rxsize = 128<<10;
            setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rxsize, sizeof(rxsize));

    #if defined(CONFIG_EXAMPLE_IPV4) && defined(CONFIG_EXAMPLE_IPV6)
            if (addr_family == AF_INET6) {
                // Note that by default IPV6 binds to both protocols, it is must be disabled
                // if both protocols used at the same time (used in CI)
                int opt = 1;
                setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
                setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
            }
    #endif

            int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            if (err < 0) {
                ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
            }
            ESP_LOGI(TAG, "Socket bound, port %d", PORT);
            sock_bound = true;
        }
        while (1) {
            if (!sock_bound) {
                vTaskDelay(1);
                break;
            };
            // ESP_LOGI(TAG, "Waiting for data");
            struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
            socklen_t socklen = sizeof(source_addr);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

            // Error occurred during receiving
            if (len < 0) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            }
            // Data received
            else {
                // // Get the sender's ip address as string
                // if (source_addr.ss_family == PF_INET) {
                //     inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
                // } else if (source_addr.ss_family == PF_INET6) {
                //     inet6_ntoa_r(((struct sockaddr_in6 *)&source_addr)->sin6_addr, addr_str, sizeof(addr_str) - 1);
                // }

                // rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string...
                // ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
                // ESP_LOGI(TAG, "%s", rx_buffer);

                // int err = sendto(sock, rx_buffer, len, 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
                // if (err < 0) {
                //     ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                //     break;
                // }

                      packet_count++;
      byte_count += len;

      int now = xTaskGetTickCount();
      if ((now - last_report) > 1000)
      {
        // Serial.println(last_report - now);
        last_report = now;
        ESP_LOGI(TAG, "core %d: packets/sec: %d", core, packet_count);
        ESP_LOGI(TAG, "core %d: bytes/sec: %d", core, byte_count);
        packet_count = 0;
        byte_count = 0;
      }
            }
        }

        if (core == 1 && sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            sock_bound = false;
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

#ifdef CONFIG_EXAMPLE_IPV4
    xTaskCreatePinnedToCore(udp_server_task, "udp_server1", 4096, (void*)0, 5, NULL, 0);
    xTaskCreatePinnedToCore(udp_server_task, "udp_server2", 4096, (void*)1, 5, NULL, 1);
#endif
#ifdef CONFIG_EXAMPLE_IPV6
    xTaskCreate(udp_server_task, "udp_server", 4096, (void*)AF_INET6, 5, NULL);
#endif

}

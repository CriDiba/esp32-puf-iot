#include "console/console.h"

#include "define.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include <stdbool.h>

static const char *TAG = "TcpConsole";

static struct ConsoleCmd console_commands[] = {
    { .name = "help",          .handler = Console_CmdHelp,         .help = "show this help message" },
    { .name = "enroll",        .handler = Console_CmdEnroll,       .help = "entroll the SRAM PUF"   },
};

void Console_CmdHelp(int argc, char *argv[]) 
{
    (void)argc;
    (void)argv;

    // TODO
    // Console_Println(ctx, "Available commands:");
    // for (size_t i = 0; i < ARRAY_SIZE(console_commands); i++) 
    //     Console_Println(ctx, "* %-15s - %s", console_commands[i].name, console_commands[i].help);

}

void Console_CmdEnroll(int argc, char *argv[])
{
    // TODO: start enroll phase
}

void Console_RunCommand(ConsoleCtx *ctx, char *command)
{
    // int argc = 0;
    // char *ptr = NULL;
    // char *argv[MAX_ARGUMENTS];
    // argv[argc++] = strtok_r(command, " \t", &ptr);
    // while (argc < MAX_ARGUMENTS && (argv[argc++] = strtok_r(NULL, " \t", &ptr)));
    // argc--;

    // if (!argv[0])
    //     return FAILURE;

    // for (size_t i = 0; i < ARRAY_SIZE(g_commands); i++) 
    // {
    //     if (!strcmp(g_commands[i].name, argv[0])) 
    //     {
    //         D("console: matched command: %s", g_commands[i].name);
    //         ErrorCode error = g_commands[i].handler(ctx, argc, argv);
    //         if (error) 
    //             Console_Println(ctx, "-ERR: %s (%d)", ErrorToString(error), error);
    //         else 
    //             Console_Println(ctx, "+OK");

    //         return error;
    //     }
    // }
    
    // Console_Println(ctx, "-ERR: Unknown command %s", argv[0]);

    // return FAILURE;
}

static void Console_GetCommand(const int socket)
{
    int length;
    char cmd[128];

    do
    {
        length = recv(socket, cmd, sizeof(cmd) - 1, 0);
        if (length > 0)
        {
            cmd[length - 1] = '\0';
            ESP_LOGI(TAG, "Received %d bytes: %s", length, cmd);
        }
    } while (length > 0);
    
    ESP_LOGI(TAG, "client disconnected");
}


void Console_TcpConsoleTask(void *pvParameters)
{
    ESP_LOGI(TAG, "open socket...");

    char client_addr_str[128];
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;
    int keepAlive = 1;
    int keepIdle = CONSOLE_KEEPALIVE_IDLE;
    int keepInterval = CONSOLE_KEEPALIVE_INTERVAL;
    int keepCount = CONSOLE_KEEPALIVE_COUNT;

    int server_socket = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (server_socket < 0) {
        ESP_LOGE(TAG, "unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = addr_family;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(CONSOLE_PORT);

    ESP_LOGI(TAG, "bind socket...");
    int bind_err = bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (bind_err != 0) {
        ESP_LOGE(TAG, "socket unable to bind: errno %d", errno);
        goto failure;
    }

    ESP_LOGI(TAG, "socket bound, port %d", CONSOLE_PORT);   

    int listen_err = listen(server_socket, 1);
    if (listen_err != 0) {
        ESP_LOGE(TAG, "error occurred during listen: errno %d", errno);
        goto failure;
    }

    while (true)
    {
        ESP_LOGI(TAG, "remote consle listening");

        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket < 0) {
            ESP_LOGE(TAG, "unable to accept connection: errno %d", errno);
            break;
        }

        /* set tcp keepalive option */ 
        setsockopt(client_socket, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
        setsockopt(client_socket, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
        setsockopt(client_socket, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
        setsockopt(client_socket, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));

        /* convert ip address to string */
        if (client_addr.sin_family == PF_INET) {
            inet_ntoa_r(((struct sockaddr_in *)&client_addr)->sin_addr, client_addr_str, sizeof(client_addr_str) - 1);
        }

        ESP_LOGI(TAG, "client connected to remote console: %s", client_addr_str);

        Console_GetCommand(client_socket);

        shutdown(client_socket, 0);
        close(client_socket);
    }

failure:
    close(server_socket);
    vTaskDelete(NULL);
}
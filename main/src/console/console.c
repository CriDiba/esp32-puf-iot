#include "console/console.h"

#include "core/core.h"
#include "crypto/crypto.h"
#include "define.h"
#include "puflib.h"

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
#include "lwip/netdb.h"

#include <stdbool.h>
#include <string.h>

#define MAX_ARGUMENTS 32

static const char *TAG = "TcpConsole";

static struct ConsoleCmd console_commands[] = {
    {.name = "help", .handler = Console_CmdHelp, .help = "show this help message"},
    {.name = "enroll", .handler = Console_CmdEnroll, .help = "entroll the SRAM PUF"},
    {.name = "puf", .handler = Console_CmdPrintPuf, .help = "print SRAM PUF buffer"},
    {.name = "challenge", .handler = Console_CmdChallenge, .help = "trigger SRAM PUF with challenge"},
    {.name = "gen_ecc_key", .handler = Console_GenEccKey, .help = "generate ECC key and print CSR"},
};

static void Console_vprintf(int socket, const char *fmt, va_list args)
{
    char buffer[256] = {0};
    int length = vsnprintf(buffer, sizeof(buffer), fmt, args);

    printf("=>> %s ", buffer);

    send(socket, buffer, length, 0);
}

void Console_Println(int socket, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Console_vprintf(socket, fmt, args);
    send(socket, NEWLINE, sizeof(NEWLINE), 0);
    va_end(args);
}

void Console_Printf(int socket, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Console_vprintf(socket, fmt, args);
    va_end(args);
}

void Console_CmdHelp(int socket, int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    Console_Println(socket, "Available commands:");
    for (size_t i = 0; i < ARRAY_SIZE(console_commands); i++)
        Console_Println(socket, "* %-15s - %s", console_commands[i].name, console_commands[i].help);
}

void Console_CmdEnroll(int socket, int argc, char *argv[])
{
    Core_EventNotify(CORE_EVENT_ENROLL);
    Console_Println(socket, "Start PUF enrollment");
}

void Console_CmdPrintPuf(int socket, int argc, char *argv[])
{
    bool puf_ok = get_puf_response();
    if (PUF_STATE == RESPONSE_READY && puf_ok)
    {
        Console_Println(socket, "PUF Bytes: %d", PUF_RESPONSE_LEN);
        for (size_t i = 0; i < PUF_RESPONSE_LEN; i++)
        {
            Console_Printf(socket, "0x%02X, ", PUF_RESPONSE[i]);
        }
        Console_Println(socket, "\n");
    }
    else
    {
        Console_Println(socket, "PUF is not ready, enroll first");
    }

    clean_puf_response();
}

void Console_CmdChallenge(int socket, int argc, char *argv[])
{
    if (argc != 2)
    {
        Console_Println(socket, "usage: %s <challenge>", argv[0]);
    }

    char chall[STR_CHALL_MAX_LEN] = {0};
    size_t length = snprintf(chall, STR_CHALL_MAX_LEN, "%s", argv[1]);
    Core_EventNotifyData(CORE_EVENT_PUF_CHALLENGE, chall, length + 1);
}

void Console_GenEccKey(int socket, int argc, char *argv[])
{
    bool puf_ok = get_puf_response();
    if (PUF_STATE == RESPONSE_READY && puf_ok)
    {
        uint8_t PUF[32] = {0};
        memcpy(PUF, PUF_RESPONSE, sizeof(PUF));

        // char *CSR = (char *)malloc(1000 * sizeof(char));

        uint16_t csr_buf_size = 500;
        uint8_t *csr_buf = (uint8_t *)calloc(csr_buf_size, sizeof(uint8_t));
        memset(csr_buf, 0, csr_buf_size);

        Crypto_GetECDSAKey(PUF, csr_buf, csr_buf_size);

        char buf[250 + 1] = {0};
        memcpy(buf, csr_buf, 250);
        Console_Printf(socket, "%s", buf);
        memcpy(buf, (csr_buf + 250), 250);
        Console_Printf(socket, "%s", buf);

        free(csr_buf);
    }
    else
    {
        Console_Println(socket, "PUF is not available");
    }

    clean_puf_response();
}

void Console_RunCommand(const int socket, char *command)
{
    int argc = 0;
    char *ptr = NULL;
    char *argv[MAX_ARGUMENTS];
    argv[argc++] = strtok_r(command, " \t", &ptr);
    while (argc < MAX_ARGUMENTS && (argv[argc++] = strtok_r(NULL, " \t", &ptr)))
        ;
    argc--;

    if (!argv[0])
        return;

    for (size_t i = 0; i < ARRAY_SIZE(console_commands); i++)
    {
        if (!strcmp(console_commands[i].name, argv[0]))
        {
            ESP_LOGI(TAG, "received command: %s", command);
            console_commands[i].handler(socket, argc, argv);
            Console_Println(socket, "+OK");
            return;
        }
    }

    Console_Println(socket, "-ERR: Unknown command %s", argv[0]);
}

static void Console_GetCommand(const int socket)
{
    int length;
    char cmd[128];

    do
    {
        Console_Printf(socket, PROMPT);
        length = recv(socket, cmd, sizeof(cmd) - 1, 0);
        if (length > 0)
        {
            cmd[length - 1] = '\0';
            Console_RunCommand(socket, cmd);
        }
    } while (length > 0);

    ESP_LOGI(TAG, "client disconnected");
}

void Console_TaskTcpConsole(void *pvParameters)
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
    if (server_socket < 0)
    {
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
    if (bind_err != 0)
    {
        ESP_LOGE(TAG, "socket unable to bind: errno %d", errno);
        goto failure;
    }

    ESP_LOGI(TAG, "socket bound, port %d", CONSOLE_PORT);

    int listen_err = listen(server_socket, 1);
    if (listen_err != 0)
    {
        ESP_LOGE(TAG, "error occurred during listen: errno %d", errno);
        goto failure;
    }

    while (true)
    {
        ESP_LOGI(TAG, "remote console listening");

        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket < 0)
        {
            ESP_LOGE(TAG, "unable to accept connection: errno %d", errno);
            break;
        }

        /* set tcp keepalive option */
        setsockopt(client_socket, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
        setsockopt(client_socket, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
        setsockopt(client_socket, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
        setsockopt(client_socket, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));

        /* convert ip address to string */
        if (client_addr.sin_family == PF_INET)
        {
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

void Console_TaskStart(void)
{
    xTaskCreate(Console_TaskTcpConsole, "tcp_console_task", 8192, NULL, uxTaskPriorityGet(NULL) + 1, NULL);
}
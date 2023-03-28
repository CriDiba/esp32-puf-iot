#pragma once

#define CONSOLE_PORT                        CONFIG_CONSOLE_PORT
#define CONSOLE_KEEPALIVE_IDLE              CONFIG_CONSOLE_KEEPALIVE_IDLE
#define CONSOLE_KEEPALIVE_INTERVAL          CONFIG_CONSOLE_KEEPALIVE_INTERVAL
#define CONSOLE_KEEPALIVE_COUNT             CONFIG_CONSOLE_KEEPALIVE_COUNT

#define PROMPT ">> "
#define LINE_BUFFER_SIZE 512

typedef void (*ConsoleCommandHandler)(int socket, int argc, char *argv[]);

struct ConsoleCmd {
    const char *name;
    const char *help;
    ConsoleCommandHandler handler;
};

/* console commands */

void Console_CmdHelp(int socket, int argc, char *argv[]);
void Console_CmdEnroll(int socket, int argc, char *argv[]);

void Console_Printf(int socket, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void Console_Println(int socket, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

void Console_RunCommand(int socket, char *command);

void Console_TcpConsoleTask(void *pvParameters);
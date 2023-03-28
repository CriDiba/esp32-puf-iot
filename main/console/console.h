#pragma once

#define CONSOLE_PORT                        CONFIG_CONSOLE_PORT
#define CONSOLE_KEEPALIVE_IDLE              CONFIG_CONSOLE_KEEPALIVE_IDLE
#define CONSOLE_KEEPALIVE_INTERVAL          CONFIG_CONSOLE_KEEPALIVE_INTERVAL
#define CONSOLE_KEEPALIVE_COUNT             CONFIG_CONSOLE_KEEPALIVE_COUNT

#define PROMPT ">> "
#define LINE_BUFFER_SIZE 512

typedef void (*ConsoleCommandHandler)(int argc, char *argv[]);

struct ConsoleCmd {
    const char *name;
    const char *help;
    ConsoleCommandHandler handler;
};

/* console commands */

void Console_CmdHelp(int argc, char *argv[]);
void Console_CmdEnroll(int argc, char *argv[]);

void Console_Printf(const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void Console_Println(const char *fmt, ...) __attribute__((format(printf, 2, 3)));

void Console_RunCommand(char *command);

void Console_TcpConsoleTask(void *pvParameters);
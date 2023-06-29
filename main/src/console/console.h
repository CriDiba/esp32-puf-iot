#pragma once

#include "core/error.h"

#define CONSOLE_PORT CONFIG_CONSOLE_PORT
#define CONSOLE_KEEPALIVE_IDLE CONFIG_CONSOLE_KEEPALIVE_IDLE
#define CONSOLE_KEEPALIVE_INTERVAL CONFIG_CONSOLE_KEEPALIVE_INTERVAL
#define CONSOLE_KEEPALIVE_COUNT CONFIG_CONSOLE_KEEPALIVE_COUNT

#define PROMPT ">> "
#define LINE_BUFFER_SIZE 512

typedef ErrorCode (*ConsoleCommandHandler)(int socket, int argc, char *argv[]);

struct ConsoleCmd
{
    const char *name;
    const char *help;
    ConsoleCommandHandler handler;
};

/* console commands */

ErrorCode Console_CmdHelp(int socket, int argc, char *argv[]);
ErrorCode Console_CmdRead(int socket, int argc, char *argv[]);
ErrorCode Console_CmdEnroll(int socket, int argc, char *argv[]);
ErrorCode Console_CmdPuf(int socket, int argc, char *argv[]);
ErrorCode Console_CmdChallenge(int socket, int argc, char *argv[]);
ErrorCode Console_CmdRefreshCrt(int socket, int argc, char *argv[]);
ErrorCode Console_CmdStoreCrt(int socket, int argc, char *argv[]);

void Console_Printf(int socket, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void Console_Println(int socket, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

void Console_RunCommand(int socket, char *command);

void Console_TaskStart(void);

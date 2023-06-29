#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "core/error.h"

struct NetworkContext;

int32_t TLSTransport_Recv(struct NetworkContext *ctx, void *pBuffer, size_t bytesToRecv);
int32_t TLSTransport_Send(struct NetworkContext *ctx, const void *pBuffer, size_t bytesToSend);
struct NetworkContext *TLSTransport_Init(const char *hostname, uint16_t port, uint32_t recvTimeout, const char *rootCaPath, const char *clientCertPath, const char *clientKeyPath);
ErrorCode TLSTransport_Connect(struct NetworkContext *ctx);
ErrorCode TLSTransport_Disconnect(struct NetworkContext *ctx, bool force);
void TLSTransport_Free(struct NetworkContext *ctx);
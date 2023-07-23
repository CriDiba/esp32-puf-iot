
#include <mbedtls/ssl.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/timing.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>

#include "esp_log.h"

#include "define.h"
#include "crypto/crypto.h"
#include "net/tls_transport.h"
#include "core/nvs.h"
#include "core/core.h"

#include <stdbool.h>
#include <string.h>

static const char *TAG = "TLS";

extern const uint8_t server_cert_pem_start[] asm("_binary_amazon_rootca_crt_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_amazon_rootca_crt_end");
// const char *client_crt = "-----BEGIN CERTIFICATE-----\nMIIBPzCB5QIJAPf/AflLn/3EMAoGCCqGSM49BAMCMB0xCzAJBgNVBAYTAklUMQ4wDAYDVQQKDAVVTklWUjAeFw0yMzA3MDQyMTM5NDRaFw0yNDA3MDMyMTM5NDRaMDIxCzAJBgNVBAYTAklUMRMwEQYDVQQDDAplc3AzMi1jcmlzMQ4wDAYDVQQKDAVVTklWUjBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABNzrRSH7qZu5DVeY6rH0Aojqwi1itWfZNIkxDpkTqOZzmAGNv9Yz4SX1BBlPHdWDdF2LSIZq6DagiJGxRzk+OoIwCgYIKoZIzj0EAwIDSQAw\nRgIhAIi1NvkvwGVOgBUUmd+YSUSiBp+3eDzCy4hyUe+7PF33AiEA7EztUGvvc+Ne1qWhczZnAYsIcWtyIBQgMFKHZ2bXDr4=\n-----END CERTIFICATE-----";

struct NetworkContext
{
    mbedtls_net_context net;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config config;
    mbedtls_x509_crt clientCertificate;
    mbedtls_x509_crt rootCertificate;
    mbedtls_pk_context privateKey;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctrDrbg;
    bool isConnected;
    const char *hostname;
    const char *rootCaPath;
    const char *clientCertPath;
    const char *clientKeyPath;
    char port[8];
    uint32_t recvTimeoutMs;
};

static void PrintError(int errorCode, const char *reason)
{
    char message[64];

    mbedtls_strerror(errorCode, message, sizeof(message));

    ESP_LOGE(TAG, "TLS: %s (%s)", reason, message);
}

int32_t TLSTransport_Recv(struct NetworkContext *ctx, void *pBuffer, size_t bytesToRecv)
{
    int result = mbedtls_ssl_read(&ctx->ssl, pBuffer, bytesToRecv);

    if (result < 0 && result != MBEDTLS_ERR_SSL_TIMEOUT)
    {
        PrintError(result, "read error");
    }

    if (result == MBEDTLS_ERR_SSL_TIMEOUT)
        return 0;

    return result;
}

int32_t TLSTransport_Send(struct NetworkContext *ctx, const void *pBuffer, size_t bytesToSend)
{
    int result = mbedtls_ssl_write(&ctx->ssl, pBuffer, bytesToSend);
    if (result < 0)
    {
        PrintError(result, "write error");
    }

    return result;
}

struct NetworkContext *TLSTransport_Init(const char *hostname, uint16_t port, uint32_t recvTimeoutMs, const char *rootCaPath, const char *clientCertPath, const char *clientKeyPath)
{
    ESP_LOGI(TAG, "transport: init with %s:%u", hostname, port);

    struct NetworkContext *ctx = calloc(1, sizeof(struct NetworkContext));
    ctx->isConnected = false;
    ctx->hostname = hostname;
    ctx->rootCaPath = rootCaPath;
    ctx->clientCertPath = clientCertPath;
    ctx->clientKeyPath = clientKeyPath;
    ctx->recvTimeoutMs = recvTimeoutMs;
    snprintf(ctx->port, sizeof(ctx->port), "%u", port);

    return ctx;
}

void TLSTransport_Free(struct NetworkContext *ctx)
{
    if (ctx->isConnected)
    {
        TLSTransport_Disconnect(ctx, true);
    }

    free(ctx);
}

ErrorCode TLSTransport_Connect(struct NetworkContext *ctx)
{
    if (ctx->isConnected)
    {
        TLSTransport_Disconnect(ctx, true);
    }

    ctx->isConnected = false;

    /* init everything upfront so we can free everything in one pass */
    mbedtls_ssl_config_init(&ctx->config);
    mbedtls_entropy_init(&ctx->entropy);
    mbedtls_ctr_drbg_init(&ctx->ctrDrbg);
    mbedtls_ssl_init(&ctx->ssl);
    mbedtls_net_init(&ctx->net);
    mbedtls_x509_crt_init(&ctx->rootCertificate);
    mbedtls_pk_init(&ctx->privateKey);

    /* init config */
    int error = mbedtls_ssl_config_defaults(&ctx->config, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (error)
    {
        PrintError(error, "failed init SSL config");
        goto err;
    }

    /* init RNG */
    error = mbedtls_ctr_drbg_seed(&ctx->ctrDrbg, mbedtls_entropy_func, &ctx->entropy, NULL, 0);
    if (error)
    {
        PrintError(error, "failed init RNG");
        goto err;
    }

    mbedtls_ssl_conf_rng(&ctx->config, mbedtls_ctr_drbg_random, &ctx->ctrDrbg);

    Buffer deviceCert;
    bool hasClientCert = Nvs_GetBuffer(Core_GetCrtNvsKey(), &deviceCert);

    if (hasClientCert)
    {
        ESP_LOGI(TAG, "cloud: loading client certificate...");
        mbedtls_x509_crt_init(&ctx->clientCertificate);

        error = mbedtls_x509_crt_parse(&ctx->clientCertificate, deviceCert.buffer, 1 + strlen((char *)deviceCert.buffer));

        // DEBUG
        // error = mbedtls_x509_crt_parse(&ctx->clientCertificate, (uint8_t *)client_crt, 1 + strlen((char *)client_crt));

        free(deviceCert.buffer);

        if (error)
        {
            PrintError(error, "failed to load client certificate");
            goto err;
        }

        ESP_LOGI(TAG, "cloud: loading client private key...");
        Crypto_GetECCKey(&ctx->privateKey);

        if (error)
        {
            PrintError(error, "failed to load private key");
            goto err;
        }

        mbedtls_ssl_conf_own_cert(&ctx->config, &ctx->clientCertificate, &ctx->privateKey);
    }

    /* init SSL */
    error = mbedtls_ssl_setup(&ctx->ssl, &ctx->config);
    if (error)
    {
        PrintError(error, "failed to setup SSL");
        goto err;
    }

    /* setup network */
    mbedtls_ssl_set_bio(&ctx->ssl, &ctx->net, mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout);
    mbedtls_ssl_conf_read_timeout(&ctx->config, 500);

    /* setup for SNI */
    error = mbedtls_ssl_set_hostname(&ctx->ssl, ctx->hostname);
    if (error)
    {
        PrintError(error, "failed to set hostname");
        goto err;
    }

    /* setup certificates */
    if (ctx->rootCaPath)
    {
        ESP_LOGI(TAG, "cloud: loading SERVER certificate...");

        // error = mbedtls_x509_crt_parse_file(&ctx->rootCertificate, ctx->rootCaPath);
        error = mbedtls_x509_crt_parse(&ctx->rootCertificate, (unsigned char *)server_cert_pem_start, 1 + strlen((const char *)server_cert_pem_start));
        if (error)
        {
            PrintError(error, "failed to load root certificate");
            goto err;
        }

        mbedtls_ssl_conf_ca_chain(&ctx->config, &ctx->rootCertificate, NULL);
        mbedtls_ssl_set_hs_ca_chain(&ctx->ssl, &ctx->rootCertificate, NULL);
    }
    else
    {
        /* disable server verification */
        mbedtls_ssl_conf_authmode(&ctx->config, MBEDTLS_SSL_VERIFY_NONE);
    }

    if (hasClientCert)
    {
        error = mbedtls_ssl_set_hs_own_cert(&ctx->ssl, &ctx->clientCertificate, &ctx->privateKey);
        if (error)
        {
            PrintError(error, "failed to set certificates");
            goto err;
        }
    }

    ESP_LOGI(TAG, "transport: connect to %s:%s", ctx->hostname, ctx->port);
    error = mbedtls_net_connect(&ctx->net, ctx->hostname, ctx->port, MBEDTLS_NET_PROTO_TCP);
    if (error)
    {
        PrintError(error, "failed to connect to endpoint");
        goto err;
    }

    error = mbedtls_ssl_handshake(&ctx->ssl);
    if (error)
    {
        PrintError(error, "failed to handshake");
        goto err;
    }

    ctx->isConnected = true;

    return SUCCESS;

err:
    mbedtls_ssl_free(&ctx->ssl);
    mbedtls_ssl_config_free(&ctx->config);
    mbedtls_x509_crt_free(&ctx->rootCertificate);
    mbedtls_x509_crt_free(&ctx->clientCertificate);
    mbedtls_pk_free(&ctx->privateKey);
    mbedtls_net_free(&ctx->net);
    mbedtls_ctr_drbg_free(&ctx->ctrDrbg);
    mbedtls_entropy_free(&ctx->entropy);

    return FAILURE;
}

ErrorCode TLSTransport_Disconnect(struct NetworkContext *ctx, bool force)
{
    if (!force)
    {
        int error = mbedtls_ssl_close_notify(&ctx->ssl);
        if (error)
            PrintError(error, "failed to close SSL");
    }

    mbedtls_ssl_free(&ctx->ssl);
    mbedtls_ssl_config_free(&ctx->config);
    mbedtls_x509_crt_free(&ctx->rootCertificate);
    mbedtls_x509_crt_free(&ctx->clientCertificate);
    mbedtls_pk_free(&ctx->privateKey);
    mbedtls_net_free(&ctx->net);
    mbedtls_ctr_drbg_free(&ctx->ctrDrbg);
    mbedtls_entropy_free(&ctx->entropy);

    ctx->isConnected = false;

    return SUCCESS;
}
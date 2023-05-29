
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

static const char *TAG = "TLS";

extern const uint8_t client_cert_pem_start[] asm("_binary_client_crt_start");
extern const uint8_t client_cert_pem_end[] asm("_binary_client_crt_end");
extern const uint8_t server_cert_pem_start[] asm("_binary_amazon_crt_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_amazon_crt_end");

// extern const uint8_t client_cert_pem_start[] asm("_binary_device_crt_start");
// extern const uint8_t client_cert_pem_end[] asm("_binary_device_crt_end");
// extern const uint8_t client_cert_key_start[] asm("_binary_device_key_start");
// extern const uint8_t client_cert_key_end[] asm("_binary_device_key_end");
// extern const uint8_t server_cert_pem_start[] asm("_binary_amazon_crt_start");
// extern const uint8_t server_cert_pem_end[] asm("_binary_amazon_crt_end");

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
    // if (bytesToRecv == 1)
    // {
    //     /* 1 byte means check if the transport is ready to receive */
    //     int pending = mbedtls_ssl_check_pending(&ctx->ssl);
    //     // if (!pending)
    //     //     /* check if there is something to read in the TCP buffer */
    //     //     pending = mbedtls_net_poll(&ctx->net, MBEDTLS_NET_POLL_READ, ctx->recvTimeoutMs) & MBEDTLS_NET_POLL_READ;

    //     if (!pending)
    //         /* nothing to read */
    //         return 0;
    // }

    int result = mbedtls_ssl_read(&ctx->ssl, pBuffer, bytesToRecv);
    if (result < 0)
        PrintError(result, "read error");

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

void TLSTransport_Connect(struct NetworkContext *ctx)
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

    // bool hasClientCert = ctx->clientCertPath != NULL && ctx->clientKeyPath != NULL;
    bool hasClientCert = true;
    if (hasClientCert)
    {
        ESP_LOGI(TAG, "cloud: loading client certificate...");
        mbedtls_x509_crt_init(&ctx->clientCertificate);

        // error = mbedtls_x509_crt_parse_file(&ctx->clientCertificate, ctx->clientCertPath);
        error = mbedtls_x509_crt_parse(&ctx->clientCertificate, (unsigned char *)client_cert_pem_start, 1 + strlen((const char *)client_cert_pem_start));

        if (error)
        {
            PrintError(error, "failed to load client certificate");
            goto err;
        }

        ESP_LOGI(TAG, "cloud: loading client private key...");
        mbedtls_pk_init(&ctx->privateKey);
        // mbedtls_pk_parse_key(&ctx->privateKey, (unsigned char *)client_cert_key_start, 1 + strlen((const char *)client_cert_key_start), NULL, 0);
        // mbedtls_pk_parse_keyfile(&ctx->privateKey, ctx->clientKeyPath, NULL);
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

    return; // SUCCESS

err:
    mbedtls_ssl_free(&ctx->ssl);
    mbedtls_ssl_config_free(&ctx->config);
    mbedtls_x509_crt_free(&ctx->rootCertificate);
    mbedtls_x509_crt_free(&ctx->clientCertificate);
    mbedtls_pk_free(&ctx->privateKey);
    mbedtls_net_free(&ctx->net);
    mbedtls_ctr_drbg_free(&ctx->ctrDrbg);
    mbedtls_entropy_free(&ctx->entropy);

    return; // FAILURE
}

void TLSTransport_Disconnect(struct NetworkContext *ctx, bool force)
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
}
#include <string.h>
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/hmac_drbg.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/sha256.h"
#include "mbedtls/platform.h"
#include "mbedtls/pk.h"
#include "mbedtls/x509_csr.h"
#include "esp_log.h"

#include "puflib.h"
#include "define.h"
#include "core/nvs.h"
#include "core/core.h"
#include "crypto/crypto.h"

static const char *TAG = "Crypto";

/* elliptic curve to use */
#define ECPARAMS MBEDTLS_ECP_DP_SECP256R1

#if !defined(ECPARAMS)
#define ECPARAMS mbedtls_ecp_curve_list()->grp_id
#endif

/* puf */
#define PUF_LENGTH 32
#define SALT_LENGTH 32

/* csr max length */
#define CSR_BUF_MAX_LEN 500

ErrorCode Crypto_SeedCtrDrbg(mbedtls_ctr_drbg_context *ctrDrbg, mbedtls_entropy_context *entropy)
{
    mbedtls_ctr_drbg_init(ctrDrbg);
    mbedtls_entropy_init(entropy);
    return mbedtls_ctr_drbg_seed(ctrDrbg, mbedtls_entropy_func, entropy, NULL, 0);
}

ErrorCode Crypto_GetRandomSalt(Buffer *outSalt)
{
    ErrorCode err = SUCCESS;

    mbedtls_ctr_drbg_context ctrDrbg;
    mbedtls_entropy_context entropy;
    err = Crypto_SeedCtrDrbg(&ctrDrbg, &entropy);
    ERROR_CHECK(err);

    err = mbedtls_ctr_drbg_random(&ctrDrbg, outSalt->buffer, outSalt->length);
    ESP_LOGI(TAG, "generate random salt: %.*s", outSalt->length, outSalt->buffer);

    mbedtls_ctr_drbg_free(&ctrDrbg);
    mbedtls_entropy_free(&entropy);

    return err;
}

ErrorCode Crypto_GetPuf(Buffer *outPuf)
{
    bool puf_ok = false;

    do
    {
        printf("Provo a recuperare la PUF");
        clean_puf_response();
        puf_ok = get_puf_response();
    } while (!puf_ok);

    if (PUF_STATE == RESPONSE_READY)
    {
        memcpy(outPuf->buffer, PUF_RESPONSE, PUF_LENGTH);
    }

    clean_puf_response();
    return SUCCESS;
}

ErrorCode Crypto_GenerateECCKey(mbedtls_pk_context *outputKey, Buffer puf, Buffer salt)
{
    assert(puf.length >= salt.length);

    ErrorCode err = SUCCESS;

    mbedtls_hmac_drbg_context hmac_drbg;
    mbedtls_hmac_drbg_init(&hmac_drbg);

    size_t seedLength = puf.length;
    uint8_t *seed = (uint8_t *)calloc(seedLength, sizeof(uint8_t));
    memset(seed, 0, seedLength);

    /* xor between puf and seed */
    for (size_t i = 0; i < puf.length; i++)
    {
        seed[i] = puf.buffer[i] ^ salt.buffer[i];
    }

    /* seed hmac drbg */
    err = mbedtls_hmac_drbg_seed_buf(&hmac_drbg, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), seed, seedLength);
    ERROR_CHECK(err);

    /* generate ecc key */
    mbedtls_pk_init(outputKey);
    mbedtls_pk_setup(outputKey, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
    err = mbedtls_ecp_gen_key(ECPARAMS, mbedtls_pk_ec(*outputKey), mbedtls_hmac_drbg_random, &hmac_drbg);
    ERROR_CHECK(err);

    /* print key pem */
    unsigned char buf[500] = {0};
    err = mbedtls_pk_write_key_pem(outputKey, buf, sizeof(buf));

    return err;
}

ErrorCode Crypto_GenerateCSR(mbedtls_pk_context *eccKey, CString certSubject, Buffer *outCsr)
{
    ErrorCode err = SUCCESS;

    mbedtls_x509write_csr signing_req;
    mbedtls_x509write_csr_init(&signing_req);

    /* set digest algo */
    mbedtls_x509write_csr_set_md_alg(&signing_req, MBEDTLS_MD_SHA256);

    /* add certificate subject name */
    err = mbedtls_x509write_csr_set_subject_name(&signing_req, certSubject.string);
    ERROR_CHECK(err);

    /* set certificate private key */
    mbedtls_x509write_csr_set_key(&signing_req, eccKey);

    /* rng */
    mbedtls_ctr_drbg_context ctrDrbg;
    mbedtls_entropy_context entropy;
    err = Crypto_SeedCtrDrbg(&ctrDrbg, &entropy);
    ERROR_CHECK(err);

    /* get crs pem */
    err = mbedtls_x509write_csr_pem(&signing_req, outCsr->buffer, outCsr->length, mbedtls_ctr_drbg_random, &ctrDrbg);
    return err;
}

ErrorCode Crypto_RefreshCertificate(Buffer *outCsr)
{
    ErrorCode err = SUCCESS;
    mbedtls_pk_context eccPkCtx;

    /* retrive puf */
    uint8_t pufBuf[PUF_LENGTH] = {0};
    Buffer puf = {.buffer = pufBuf, .length = sizeof(pufBuf)};
    err = Crypto_GetPuf(&puf);
    ERROR_CHECK(err);

    /* generate client certificate */
    ESP_LOGI(TAG, "generate client certificate");
    char certificateSubject[100] = {0};
    const char *commonName = DEVICE_ID;
    const char *organization = CERT_ORGANIZATION;
    size_t len = snprintf(certificateSubject, sizeof(certificateSubject), "C=IT,CN=%s,O=%s", commonName, organization);
    CString subject = {.string = certificateSubject, .length = len};

    /* generate random salt */
    ESP_LOGI(TAG, "generate random salt");
    uint8_t saltBuf[SALT_LENGTH] = {0};
    Buffer salt = {.buffer = saltBuf, .length = sizeof(saltBuf)};
    err = Crypto_GetRandomSalt(&salt);
    ERROR_CHECK(err);
    Nvs_SetBuffer(NVS_DEVICE_SALT_KEY_TMP, salt);

    /* generate keypair */
    ESP_LOGI(TAG, "generate ECC keypair");
    err = Crypto_GenerateECCKey(&eccPkCtx, puf, salt);
    ERROR_CHECK(err);

    /* generate certificate signing request*/
    uint8_t *csrBuf = (uint8_t *)calloc(CSR_BUF_MAX_LEN, sizeof(uint8_t));
    memset(csrBuf, 0, CSR_BUF_MAX_LEN);
    outCsr->buffer = csrBuf;
    outCsr->length = CSR_BUF_MAX_LEN;

    /* generate csr */
    ESP_LOGI(TAG, "generate CSR from keypair");
    err = Crypto_GenerateCSR(&eccPkCtx, subject, outCsr);
    ERROR_CHECK(err);
    Nvs_SetBuffer(NVS_DEVICE_CSR_KEY_TMP, *outCsr);

    return err;
}

ErrorCode Crypto_GetECCKey(mbedtls_pk_context *eccKey)
{
    ErrorCode err = SUCCESS;

    Buffer salt;
    bool findSalt = Nvs_GetBuffer(Core_GetSaltNvsKey(), &salt);
    if (!findSalt)
    {
        ESP_LOGE(TAG, "salt not stored");
        return FAILURE;
    }

    /* retrive puf */
    uint8_t pufBuf[PUF_LENGTH] = {0};
    Buffer puf = {.buffer = pufBuf, .length = sizeof(pufBuf)};
    err = Crypto_GetPuf(&puf);
    ERROR_CHECK(err);

    err = Crypto_GenerateECCKey(eccKey, puf, salt);

    return err;
}
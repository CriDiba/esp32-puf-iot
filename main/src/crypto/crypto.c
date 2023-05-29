#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/hmac_drbg.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/sha256.h"
#include "mbedtls/platform.h"
#include "mbedtls/pk.h"

#include "puflib.h"

#include "mbedtls/x509_csr.h" // mbedtls_x509write_csr

#include <string.h>

#include "esp_log.h"

static const char *TAG = "Crypto";

/* elliptic curve to use */
#define ECPARAMS MBEDTLS_ECP_DP_SECP256R1

#if !defined(ECPARAMS)
#define ECPARAMS mbedtls_ecp_curve_list()->grp_id
#endif

void Crypto_GenECCKey(mbedtls_pk_context *ecc_key, uint8_t *puf)
{
    mbedtls_hmac_drbg_context hmac_drbg;
    mbedtls_hmac_drbg_init(&hmac_drbg);

    // const unsigned char seed[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    // size_t seed_length = sizeof(seed);
    size_t seed_length = 32;

    mbedtls_hmac_drbg_seed_buf(&hmac_drbg, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), puf, seed_length);

    mbedtls_pk_init(ecc_key);
    mbedtls_pk_setup(ecc_key, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));

    mbedtls_ecp_gen_key(ECPARAMS, mbedtls_pk_ec(*ecc_key), mbedtls_hmac_drbg_random, &hmac_drbg);

    unsigned char buf[500] = {0};
    mbedtls_pk_write_key_pem(ecc_key, buf, sizeof(buf));
    // printf("\n%s\n", buf);
}

void Crypto_GenCSR(mbedtls_pk_context *ecc_key, uint8_t *csr_buf, uint16_t csr_buf_size)
{
    mbedtls_x509write_csr signing_req;
    mbedtls_x509write_csr_init(&signing_req);

    mbedtls_x509write_csr_set_md_alg(&signing_req, MBEDTLS_MD_SHA256);

    /* add certificate subject name */
    char *subject_name = "CN=Cert,O=mbed TLS,C=UK\n";
    mbedtls_x509write_csr_set_subject_name(&signing_req, subject_name);

    /* set certificate private key */
    mbedtls_x509write_csr_set_key(&signing_req, ecc_key);

    /* rng */
    mbedtls_hmac_drbg_context hmac_drbg;
    mbedtls_hmac_drbg_init(&hmac_drbg);
    mbedtls_entropy_context entropy;
    mbedtls_entropy_init(&entropy);
    mbedtls_hmac_drbg_seed(&hmac_drbg, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), mbedtls_entropy_func, &entropy, NULL, 0);

    /* get crs pem */
    mbedtls_x509write_csr_pem(&signing_req, csr_buf, csr_buf_size, mbedtls_hmac_drbg_random, &hmac_drbg);
    printf("\n%s\n", csr_buf);
}

void Crypto_GetECDSAKey(uint8_t *puf, uint8_t *csr_buf, uint16_t csr_buf_size)
{
    mbedtls_pk_context ecc_key;

    ESP_LOGI(TAG, "Generate ECC KEY");
    Crypto_GenECCKey(&ecc_key, puf);

    ESP_LOGI(TAG, "Generate CSR");
    Crypto_GenCSR(&ecc_key, csr_buf, csr_buf_size);
}

void Crypto_GetECCKey(mbedtls_pk_context *ecc_key)
{
    bool puf_ok = false;

    do
    {
        clean_puf_response();
        puf_ok = get_puf_response();
    } while (!puf_ok);

    if (PUF_STATE == RESPONSE_READY)
    {
        uint8_t PUF[32] = {0};
        memcpy(PUF, PUF_RESPONSE, sizeof(PUF));
        Crypto_GenECCKey(ecc_key, PUF);
    }
    else
    {
        ESP_LOGI(TAG, "PUF is not available");
    }

    clean_puf_response();
}
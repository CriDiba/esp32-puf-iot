#pragma once

#include "mbedtls/pk.h" // mbedtls_pk_context

void Crypto_GetECDSAKey(uint8_t *puf, uint8_t *csr_buf, uint16_t csr_buf_size);

void Crypto_GetECCKey(mbedtls_pk_context *ecc_key);
#pragma once

#include "mbedtls/pk.h"
#include "core/error.h"
#include "define.h"

ErrorCode Crypto_GetECCKey(mbedtls_pk_context *eccKey);
ErrorCode Crypto_RefreshCertificate(Buffer *outCsr);
ErrorCode Crypto_GetRandomSalt(Buffer *outSalt);
ErrorCode Crypto_GetPuf(Buffer *outPuf);
#!/bin/bash

ROOTCA_KEY_FILENAME=rootca_key.key
ROOTCA_CERT_FILENAME=rootca_cert.pem

DEVICE_CERT_FILENAME=device_cert.pem

if [ $# -lt 1 ]; then
  echo "Usage: $0 <device_csr_filename>"
  exit 1
fi

csr_filename=$1

if [ -f "$csr_filename" ]; then

  openssl x509 -req \
    -days 365 -sha256 \
    -CA $ROOTCA_CERT_FILENAME \
    -CAkey $ROOTCA_KEY_FILENAME \
    -CAcreateserial \
    -in $csr_filename \
    -out $DEVICE_CERT_FILENAME

else
    echo "File $csr_filename does not exist."
    exit 1
fi
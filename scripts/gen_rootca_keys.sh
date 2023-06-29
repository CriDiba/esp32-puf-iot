#!/bin/bash

ROOTCA_KEY_FILENAME=rootca_key.key
ROOTCA_CERT_FILENAME=rootca_cert.pem

# find curve
# openssl ecparam -list_curves

# generate a private key for a curve
openssl ecparam -name prime256v1 -genkey -noout -out $ROOTCA_KEY_FILENAME

# generate corresponding public key
# openssl ec -in private-key.pem -pubout -out public-key.pem

# create a self-signed certificate
openssl req -x509 -new -nodes -sha256 -days 365 -config rootca_openssl.conf -extensions v3_ca -key $ROOTCA_KEY_FILENAME -out $ROOTCA_CERT_FILENAME

# register CA certificate on aws
aws iot register-ca-certificate --ca-certificate file://$ROOTCA_CERT_FILENAME \
    --certificate-mode SNI_ONLY --set-as-active --allow-auto-registration \
    --registration-config file://jitp_template.json \
    --region eu-west-1 --profile iotinga

#!/bin/bash

if [ $# -lt 1 ]; then
  echo "Usage: $0 <arg1>"
  exit 1
fi

operation=$1

if [ "$operation" = "sign" ]
then
    echo "Client ID: "; read client_id
    echo "Csr file: "; read csr_filename

    # read csr file
    csr=""
    if [ -f "$csr_filename" ]; then
        # file exists
        while read line; do
            csr+=$line
        done < $csr_filename
    else
        echo "File $csr_filename does not exist."
        exit 1
    fi

    EVENT_PAYLOAD=$(printf '{"clientid": "%s", "csr": "%s", "manual": true}' "$client_id" "$csr")

    aws lambda invoke \
        --function-name certRotationFn \
        --payload "$EVENT_PAYLOAD" \
        --cli-binary-format raw-in-base64-out \
        aws_response.json
fi

if [ "$operation" = "refresh" ]
then
    echo "Client ID: "; read client_id
    echo "Certificate Id: "; read certificate_id

    EVENT_PAYLOAD=$(printf '{"clientid": "%s", "certificateId": "%s"}' "$client_id" "$certificate_id")

    aws lambda invoke \
        --function-name certRotationFn \
        --payload "$EVENT_PAYLOAD" \
        --cli-binary-format raw-in-base64-out \
        aws_response.json
fi
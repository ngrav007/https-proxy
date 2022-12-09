#!/bin/bash

# Generates a secure key for a certificate, and stores it in the keys directory in
# the workspaces directory.

# Usage: generate_secure_key.sh <common name>

source /workspaces/Development/https-proxy/scripts/config/.config

function generate_key() 
{

    if [ -z "${1}" ]; then
        echo "Usage: $0 <common name>"
        exit 1
    fi
    CN="${1}"

    # Check for sudo privileges
    if [[ "${EUID}" -ne 0 ]]; then
        echo "[!] This script must be run as root" 1>&2
        exit 1
    fi

    # Generate password
    PASSWD="${DEFAULT_DEV_PASSWD}"

    # Create Password File
    PASSWD_FILE="${PASSWD_DIR}/${CN}.passwd"

    # Create Key File
    KEY_FILE="${KEYS_DIR}/${CN}.key"
    SECURE_KEY="${KEYS_DIR}/${CN}.key.secure"
    INSECURE_KEY="${KEYS_DIR}/${CN}.key.insecure"

    # Save password to file
    echo "${PASSWD}" > "${PASSWD_FILE}"

    # Generate key
    echo "[*] Generating secure key for ${CN} in ${SECURE_KEY}"
    openssl genrsa -des3 -out "${SECURE_KEY}" -passout pass:"${PASSWD}" 2048
    
    # Create insecure key so we can sign certs without a password
    echo "[*] Generating insecure key for ${CN} in ${INSECURE_KEY}"
    openssl rsa -in "${SECURE_KEY}" -out "${INSECURE_KEY}" -passin pass:"${PASSWD}"

    # Move insecure key to be the key
    mv "${INSECURE_KEY}" "${KEY_FILE}"

}

# Generate password file
generate_key "$1"

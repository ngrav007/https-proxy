#!/bin/bash

# Generate a password file for a dev certificate
# Usage: generate_dev_KEY.sh <filename>

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

    # Create password File
    KEY_FILE="${KEYS_DIR}/${CN}.key"

    # Generate password
    KEY="${DEFAULT_DEV_KEY}"

    echo "[*] Generating key for ${CN} in ${KEY_FILE}"


    # Write password to file
    echo "${KEY}" > "${KEY_FILE}"
}

# Generate password file
generate_key "$1"

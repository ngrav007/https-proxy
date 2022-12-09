#!/bin/bash

# Generate a password file for a dev certificate
# Usage: generate_dev_key.sh <filename>

source /workspaces/Development/https-proxy/scripts/config/.config

# Check for sudo privileges
if [[ $EUID -ne 0 ]]; then
   echo "[!] This script must be run as root" 1>&2
   exit 1
fi

# Get Common Name
if [ -z "$1" ]; then
    echo "Usage: $0 <filename>"
    exit 1
fi
# CN=$1

# Create Key File
# KEY_FILE=${KEYS_DIR}/${CN}.key
# if [ -f ${KEYS_FILE} ]; then
#     rm ${KEY_FILE}
# fi

# Generate password
KEY=DEFAULT_DEV_PASSWD

# Write password to file
echo ${KEY} > $1

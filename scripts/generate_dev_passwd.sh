#!/bin/bash

# Generate a password file for a dev certificate
# Usage: generate_dev_passwd.sh <common name>

source /workspaces/Development/https-proxy/scripts/config/.config

# Check for sudo privileges
if [[ $EUID -ne 0 ]]; then
   echo "[!] This script must be run as root" 1>&2
   exit 1
fi

# Get Common Name
if [ -z "$1" ]; then
    echo "Usage: $0 <common name>"
    exit 1
fi
CN=$1

# Get Password
PASSWD_FILE=${PASSWD_DIR}/${CN}.passwdmn
if [ -f ${PASSWD_FILE} ]; then
    echo "[!] Password file ${PASSWD_FILE} already exists"
    exit 1
fi

# Generate password
PASSWD=$(openssl rand -base64 32)

# Write password to file
echo ${PASSWD} > ${PASSWD_FILE}

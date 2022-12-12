#!/bin/bash

# Add the given host to an existing certificate
# Usage: generate_ext_file.sh <common name> <domain name>

source /workspaces/Development/https-proxy/scripts/config/.config

# External Scripts
GENERATE_CERT="/workspaces/Development/https-proxy/scripts/generate_cert.sh"

# Get Domain Name
if [ -z "$1" ]; then
    echo "Usage: add_host_to_cert.sh <domain name>"
    exit -1
fi
DOMAIN_NAME=$1

# Check for sudo privileges
if [[ $EUID -ne 0 ]]; then
   echo "[!] This script must be run as root" 1>&2
   exit -1
fi

# Ensure Root CA exists
if [ ! -f "${ROOTCA_CRT}" ]; then
    echo "[!] Root CA certificate ${ROOTCA_CRT} not found"
    exit -1
fi

# Ensure Root CA Key exists
if [ ! -f "${ROOTCA_KEY}" ]; then
    echo "[!] Root CA key ${ROOTCA_KEY} not found"
    exit -1
fi

if [ ! -f "${PROXY_EXT}" ]; then
    echo "[!] Proxy extension file ${PROXY_EXT} not found"
    exit -1
fi

# Check if domain name already exists in the proxy config
if grep -q ${DOMAIN_NAME} ${PROXY_EXT}; then
    echo "[!] Domain name ${1} already exists in ${PROXY_EXT}"
    exit -1
fi

# Add domain name to the proxy configuration file
echo "[*] Adding ${DOMAIN_NAME} to ${PROXY_EXT}"
if [ -z "${PROXY_DNS_COUNT}" ]; then
    echo "[!] PROXY_DNS_COUNT not set in ${CONFIG_FILE}"
    exit -1
fi

# Write domain name to proxy config
DOMAIN_STRING="DNS.${PROXY_DNS_COUNT} = ${DOMAIN_NAME}"
echo "PROXY EXTENSION FILE: ${PROXY_EXT}"
echo "PROXY DNS COUNT: ${PROXY_DNS_COUNT}"
echo "DOMAIN STRING: ${DOMAIN_STRING}"
echo "${DOMAIN_STRING}" >> "${PROXY_EXT}"

# Increment PROXY_DNS_COUNT in config file
PROXY_DNS_COUNT=$(( ${PROXY_DNS_COUNT} + 1 ))
CONFIG_FILE="/workspaces/Development/https-proxy/scripts/config/.config"
sed -i "s/PROXY_DNS_COUNT=.*/PROXY_DNS_COUNT=${PROXY_DNS_COUNT}/g" ${CONFIG_FILE}

# Generate new proxy certificate
${GENERATE_CERT} ${PROXY_CN}

exit 1

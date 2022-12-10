#!/bin/bash

source /workspaces/Development/https-proxy/scripts/config/.config

# This script is used to verify the certificate of a remote host.
# It is used by the https-proxy script to verify the certificate of the
# remote host.  It is also used by the https-proxy-verify-cert script
# to verify the certificate of the remote host.

# Check for sudo privileges
if [[ $EUID -ne 0 ]]; then
   echo "[!] This script must be run as root" 1>&2
   exit 1
fi

KEY="${ROOTCA_KEY}"
CRT="${ROOTCA_CRT}"
PASSWD=$(cat "${ROOTCA_PASSWD}")

# Verify certificate
echo "[*] Verifying certificate ${CRT}"
openssl verify -CAfile "${CRT}" "${CRT}"
awk -v cmd='openssl x509 -noout -subject' '/BEGIN/{close(cmd)};{print | cmd}' < /etc/ssl/certs/ca-certificates.crt | grep "${ROOTCA_CN}"


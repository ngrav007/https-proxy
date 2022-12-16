#!/bin/bash

source /workspaces/Development/http-proxy/scripts/config/.config

# Check for sudo privileges
if [[ $EUID -ne 0 ]]; then
   echo "[!] This script must be run as root" 1>&2
   exit 1
fi

# Get Password
# PASSWD_FILE=${PASSWD_DIR}/${ROOTCA_CN}.passwd
# if [ ! -f ${PASSWD_FILE} ]; then
#     echo "[!] Password file ${PASSWD_FILE} not found"
#     exit 1
# fi



# Set key, certificate (PEM), and password from file
KEY="${ROOTCA_KEY}"
CRT="${ROOTCA_CRT}"
PASSWD=$(cat "${ROOTCA_PASSWD}")

echo "[+] Generating certificate for ${CN} in ${CERTDIR}"
echo "       Root CA: ${CRT}"
echo "           Key: ${KEY}"
echo "      Password: ${PASSWD}"

# # If key exists, remove it
# if [ -f "${KEY}" ]; then
#     echo "[!] Removing existing ${KEY} from local workspace"
#     rm "${KEY}"
# fi

# # If certificate exists, remove it
# if [ -f "${CRT}" ]; then
#     echo "[!] Removing existing certificate from local workspace"
#     rm "${CRT}"
# fi

# Generate certificate key include password - store in /workspaces/Development/https-proxy/etc/private
# To make a key without a password, remove the -des3 option
echo "[*] Generating certificate key"
openssl genrsa -des3 -out "${KEY}" -passout "file:${ROOTCA_PASSWD}" 2048

# Generate certificate root certificate
echo "[*] Generating root certificate"
openssl req -x509 -new -nodes -key ${KEY} -sha256 -days 3650 -out ${CRT} -subj "${SUBJECT_ROOT}" -passin "pass:${PASSWD}"

# Add certificate to ca-certificates, this will update the ca-certificates.crt file and add the
# certificate to the trusted store
echo "[*] Copying certificate to ${CA_CERTS_DIR}"
cp "${CRT}" "${CA_CERTS_DIR}/${ROOTCA_CN}.crt"
echo "[*] Updating trusted certificates"
update-ca-certificates

# Verify certificate
echo "[*] Verifying certificate ${CRT}"
openssl verify -CAfile "${CRT}" "${CRT}"
awk -v cmd='openssl x509 -noout -subject' '/BEGIN/{close(cmd)};{print | cmd}' < /etc/ssl/certs/ca-certificates.crt | grep "${ROOTCA_CN}"

#!/bin/bash

source /workspaces/Development/https-proxy/scripts/config/.config

source /workspaces/Development/https-proxy/scripts/generate_dev_passwd.sh
  
# Check for sudo privileges
if [[ $EUID -ne 0 ]]; then
   echo "[!] This script must be run as root" 1>&2
   exit 1
fi

# Ensure Root CA exists
if [ ! -f "${ROOT_CA_CRT}" ]; then
    echo "[!] Root CA certificate ${ROOT_CA_CRT} not found"
    exit 1
fi

# Ensure Root CA Key exists
if [ ! -f "${ROOT_CA_KEY}" ]; then
    echo "[!] Root CA key ${ROOT_CA_KEY} not found"
    exit 1
fi

# Get Common Name
if [ -z "$1" ]; then
    echo "Usage: $0 <common name>"
    exit 1
fi
CN=$1

# Key File
KEY=${KEYS_DIR}/${CN}.key
if [ -f ${KEY} ]; then
    echo "[!] Key ${KEY} already exists, removing..."
    rm ${KEY} # TODO - should we always remove?
    generate_dev_passwd.sh ${CN}
elif [ ! -f ${KEY} ]; then
    echo "[!] Key ${KEY} does not exist"
    generate_dev_passwd.sh ${CN}
fi

# Certificate Signing Request
CSR=${CERTS_DIR}/${CN}.csr
if [ -f ${CSR} ]; then
    echo "[!] Certificate Signing Request ${CSR} already exists"
    rm ${CSR} # TODO - should we always remove?
fi

# Certificate
CRT=${CERTS_DIR}/${CN}.crt
if [ -f ${CRT} ]; then
    echo "[!] Certificate ${CRT} already exists"
    rm ${CRT} # TODO - should we always remove?
fi

# Extension file
EXT_FILE=${EXT_DIR}/${CN}.ext
if [ ! -f ${EXT_FILE} ]; then
    echo "[!] Extension file ${EXT_FILE} not found"
    exit 1
fi

# # If key exists, remove it
# if [ -f "${KEY}" ]; then
#     echo "Removing existing key"
#     # Remove local key
#     rm "${KEY}"
#     # # TODO - should we remove key from trusted store?
#     # rm "/etc/ssl/private/${CN}.key"
# fi

# # If certificate exists, remove it
# if [ -f "${CRT}" ]; then
#     echo "Removing existing certificate"
#     # Remove local certificate
#     rm "${CRT}"
#     # # TODO - should we remove certificate from ca-certificates?
#     # rm "/usr/share/ca-certificates/${CN}.crt"
#     # # TODO - should we remove certificate from trusted store?
#     # rm "/etc/ssl/certs/${CN}.crt"
# fi

echo "[*] Generating certificate for ${CN} in ${CERTS_DIR}"
echo " -------------------------------------------------- "
echo " Root CA Certificate: ${ROOT_CA_CRT}"
echo "         Root CA Key: ${ROOT_CA_KEY}"
echo " -------------------------------------------------- "
echo "                  CN: ${CN}"
echo "                 Key: ${KEY}"
echo "     Signing Request: ${CSR}"
echo "      Extension File: ${EXT_FILE}"
echo "         Certificate: ${CRT}"
echo " -------------------------------------------------- "

# Generate key for new certificate
openssl genrsa -out ${KEY} 2048

# Generate certificate signing request
openssl req -new -key ${KEY} -out ${CSR} -subj "${SUBJECT}${CN}"

# Generate certificate
openssl x509 -req -in ${CSR} -CA ${ROOT_CA_CRT} -CAkey ${ROOT_CA_KEY} -CAcreateserial -out ${CRT} -days 825 -sha256 -extfile ${EXT_FILE} -passin file:${ROOT_CA_PASSWD}

# TODO - do we do any of this below for dev certs?
# # Add certificate to ca-certificates
# echo "[*] Updating ca-certificates"
# cp "${CRT}" "/usr/share/ca-certificates/${CN}.crt"
# update-ca-certificates

# # Add key to trusted store
# echo "[*] Updating trusted store"
# cp "${KEY}" "/etc/ssl/private/${CN}.key"

# # Update permissions
# echo "[*] Updating permissions of key"
# chown root:root ${TRUSTED_PRIVATE_KEY_DIR}/${CN}.key
# chmod 0600 ${TRUSTED_PRIVATE_KEY_DIR}/${CN}.key

# # Verify certificate
# echo "[*] Verifying certificate"
# openssl verify -CAfile "${CRT}" "${CRT}"
# awk -v cmd='openssl x509 -noout -subject' '/BEGIN/{close(cmd)};{print | cmd}' < /etc/ssl/certs/ca-certificates.crt | grep eregion

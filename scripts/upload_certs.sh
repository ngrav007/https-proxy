#!/bin/bash

# This script is used to upload the certificate and key to the server and 
# install them in the /etc/ssl directory of the server. This script is
# intended to be run on the server. It will prompt for the password to 
# decrypt the certificate and key.

echo "A work in progress...might not even need it."

# source /workspaces/Development/https-proxy/scripts/config/.config

# # Check for sudo privileges
# if [[ $EUID -ne 0 ]]; then
#    echo "This script must be run as root" 1>&2
#    exit 1
# fi

# # Get the password for the certificate
# WORKDIR=$(pwd)
# PASSWORD_FILE=${WORKDIR}/config/eregion-cert.passwd
# if [ ! -f ${PASSWORD_FILE} ]; then
#     echo "Password file ${PASSWORD_FILE} not found"
#     exit 1
# fi

# # Read password from file
# PASSWORD=$(cat ${PASSWORD_FILE})

# # Get the certificate and key
# CRT_FILE=${WORKDIR}/../etc/certs/eregion.localhost.crt
# KEY_FILE=${WORKDIR}/../etc/certs/eregion.localhost.key

# # Check that the certificate and key exist
# if [ ! -f ${CRT_FILE} ]; then
#     echo "Certificate file ${CRT_FILE} not found"
#     exit 1
# fi

# if [ ! -f ${KEY_FILE} ]; then
#     echo "Key file ${KEY_FILE} not found"
#     exit 1
# fi

# # Decrypt the certificate and key
# openssl rsa -in ${KEY_FILE} -out ${KEY_FILE}.dec -passin pass:${PASSWORD}
# openssl x509 -in ${CRT_FILE} -out ${CRT_FILE}.dec -passin pass:${PASSWORD}

# # Uninstall the certificate and key if they already exist
# if [ -f /etc/ssl/certs/eregion.localhost.crt ]; then
#     rm /etc/ssl/certs/eregion.localhost.crt
# fi

# if [ -f /etc/ssl/private/eregion.localhost.key ]; then
#     rm /etc/ssl/private/eregion.localhost.key
# fi

# # Install the certificate and key
# cp ${CRT_FILE}.dec /etc/ssl/certs/eregion.localhost.crt
# cp ${KEY_FILE}.dec /etc/ssl/private/eregion.localhost.key

# # Remove the decrypted files if flag is set
# if [ "$1" == "--remove" ]; then
#     rm ${CRT_FILE}.dec
#     rm ${KEY_FILE}.dec
# fi

# # Change the permissions on the certificate and key
# chmod 644 /etc/ssl/certs/eregion.localhost.crt
# chmod 600 /etc/ssl/private/eregion.localhost.key

# # Change the owner of the certificate and key
# chown root:root /etc/ssl/certs/eregion.localhost.crt
# chown root:root /etc/ssl/private/eregion.localhost.key

# # Change the permissions on the certificate and key
# chmod 644 /etc/ssl/certs/eregion.localhost.crt
# chmod 600 /etc/ssl/private/eregion.localhost.key

# # Change the owner of the certificate and key
# chown root:root /etc/ssl/certs/eregion.localhost.crt
# chown root:root /etc/ssl/private/eregion.localhost.key


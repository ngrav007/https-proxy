#!/bin/bash

# This script is used to install OpenSSL on a Ubuntu 22.04 system and configure it
LATEST="3.0.7"

# Make sure we have sudo privileges
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

# Change into the /tmp directory
cd /tmp

# Check for command line arguments
VERSION=""
if [ $# == 0 ]; then
        echo "What version of OpenSSL do you want to install?"
        read VERSION
        RELEASE=$(echo ${VERSION} | cut -b1-5)
elif [ $1 == "--latest" ]; then
        VERSION=${LATEST}
elif [ $1 == "--version" ]; then
        VERSION=$2
        RELEASE=$(echo ${VERSION} | cut -b1-5)
elif [ [ $1 == "--help" || $1 == "-h" ]]; then
        echo "Usage: install_openssl.sh [OPTION]"
        echo "Install OpenSSL on a Ubuntu 22.04 system"
        echo ""
        echo "Options:"
        echo "  --latest           Install the latest version of OpenSSL"
        echo "  --version VERSION  Install a specific version of OpenSSL"
        echo "  --help             Display this help and exit"
        exit 0
elif [ $# -gt 0 ]; then 
        echo "Invalid option: $1"
        echo "Try 'install_openssl.sh --help' for more information."
        exit 1
fi

if [[ ${VERSION} == "" || ${VERSION} == ${LATEST} ]]; then
        VERSION=${LATEST}
        echo "Installing latest version of OpenSSL: ${VERSION}"
        wget "https://www.openssl.org/source/openssl-${VERSION}.tar.gz" -O "/tmp/openssl-${VERSION}.tar.gz"
        if [ $? -ne 0 ]; then
                echo "Failed to download OpenSSL version ${VERSION}"
                exit 1
        fi
else
        echo "Installing OpenSSL Version ${VERSION}"
        wget "https://www.openssl.org/source/old/openssl-${VERSION}.tar.gz" -O "/tmp/openssl-${VERSION}.tar.gz"
        if [ $? -ne 0 ]; then
                echo "Failed to download OpenSSL version ${VERSION}"
                exit 1
        fi
fi

# Extract the source code
tar -xvf "/tmp/openssl-${VERSION}.tar.gz" -C /opt

# Change into the OpenSSL source code directory
cd "/opt/openssl-${VERSION}"

# Configure OpenSSL

./config "--prefix=/opt/openssl --openssldir=/opt/openssl -L/opt/openssl/lib -I/opt/openssl/include \
         -Wl,-rpath,/opt/openssl/lib -Wl,-rpath-link,/opt/openssl/lib -Wl,-rpath,/usr/local/lib     \
         -Wl,-rpath-link,/usr/local/lib -DOPENSSL_TLS_SECURITY_LEVEL=2 enable-ec_nistp_64_gcc_128"


# Build OpenSSL
make
if [ $? -ne 0 ]; then
        echo "Failed to build OpenSSL"
        exit 1
fi

make test
if [ $? -ne 0 ]; then
        echo "Failed to test OpenSSL"
        exit 1
fi

make install
if [ $? -ne 0 ]; then
        echo "Failed to install OpenSSL"
        exit 1
fi

# Create a symbolic link to the latest version of OpenSSL
ln -s "/opt/openssl-${VERSION}" "/opt/openssl"

# Add OpenSSL to the system path
echo 'export PATH=/opt/openssl/bin:${PATH}' >> ~/.bashrc

# Add OpenSSL to the system library path
echo 'export LD_LIBRARY_PATH=/opt/openssl/lib:${LD_LIBRARY_PATH}' >> ~/.bashrc

# Add OpenSSL to Package Config path
echo 'export PKG_CONFIG_PATH=/opt/openssl/lib/pkgconfig:${PKG_CONFIG_PATH}' >> ~/.bashrc

# Set OpenSSL Root Directory
echo 'export OPENSSL_ROOT_DIR=/opt/openssl' >> ~/.bashrc

# Set OpenSSL Lib Directory
echo 'export OPENSSL_LIB_DIR=/opt/openssl/lib' >> ~/.bashrc

# Set OpenSSL Include Directory
echo 'export OPENSSL_INCLUDE_DIR=/opt/openssl/include' >> ~/.bashrc

# Set Compiler Flags
echo 'export CFLAGS=-I/opt/openssl/include:${CFLAGS}' >> ~/.bashrc
echo 'export LDFLAGS=-L/opt/openssl/lib:${LDFLAGS}' >> ~/.bashrc

# Reload the bashrc file
source ~/.bashrc

# Use system CA certificates for OpenSSL
cd /opt/openssl
rm -rf certs
ln -s /etc/ssl/certs certs

# Change back to the home directory
cd ~

# Check the OpenSSL version
openssl version -a

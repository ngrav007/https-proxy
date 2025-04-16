# HTTPS Proxy

A C-based implementation of an HTTP/HTTPS proxy server with SSL/TLS support, featuring caching capabilities and content filtering.

## Overview

This project implements a fully functional HTTP/HTTPS proxy server in C, with emphasis on network programming using sockets and OpenSSL for secure communications. The proxy supports both standard HTTP and encrypted HTTPS connections, certificate validation, content caching, and URL filtering.

## Project Structure

- **proxy/**: Main codebase
  - **src/**: Core implementation files
    - `proxy.c`: Main proxy implementation with connection handling and SSL support
    - `http.c`: HTTP protocol implementation (request/response parsing)
    - `cache.c`: Caching mechanism implementation
    - `client.c`: Client connection handling
  - **include/**: Header files
  - **bin/**: Compiled binaries
  - **client/**: HTTP client implementation
  - **server/**: HTTP server implementation
  - **tests/**: Test cases

- **scripts/**: Helper scripts
  - Certificate generation and management scripts
  - Proxy control scripts
  - OpenSSL installation scripts

- **etc/**: Test credentials and certificates
  - **certs/**: SSL/TLS certificates
  - **private/**: Private keys
  - **csr/**: Certificate signing requests
  - **ext/**: Certificate extensions

## Features

- **HTTP Proxy**: Standard HTTP proxy functionality
- **HTTPS Proxy**: Support for encrypted HTTPS connections
- **SSL/TLS Support**: Certificate validation and secure tunneling
- **Content Caching**: Efficient caching of HTTP responses
- **URL Filtering**: Ability to block specific domains or URLs
- **Error Handling**: Comprehensive error detection and reporting
- **Connection Management**: Efficient handling of multiple concurrent connections

## Building the Project

```bash
# Navigate to the proxy directory
cd proxy

# Build all components
make all
```

This will compile the proxy server, HTTP client, HTTP server, and TLS client.

## Running the Proxy

```bash
# Run the proxy server (default port is defined in the code)
./bin/proxy

# To stop the proxy
../scripts/halt-proxy.sh
```

## SSL/TLS Certificate Management

The project includes several scripts for managing SSL/TLS certificates:

```bash
# Generate a root CA certificate
./scripts/generate_rootca.sh

# Generate a proxy certificate
./scripts/generate_proxy_cert.sh

# Generate a client certificate
./scripts/generate_client_cert.sh

# Update proxy certificate for a specific domain
./scripts/update_proxy_cert.sh [domain]

# Display certificate information
./scripts/display_cert.sh [certificate_file]
```

## Implementation Details

The proxy server implements the following core functionalities:

1. **HTTP Request Handling**: Parsing and forwarding HTTP requests
2. **HTTPS Tunneling**: Establishing secure tunnels for HTTPS connections
3. **Response Caching**: Storing and serving cached responses
4. **Certificate Validation**: Verifying server certificates for HTTPS connections
5. **URL Filtering**: Blocking requests to specific domains

## Dependencies

- C compiler (gcc recommended)
- OpenSSL library for SSL/TLS support
- Standard POSIX libraries

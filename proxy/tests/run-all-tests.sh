#!/bin/bash

source /workspaces/Development/http-proxy/proxy/tests/.test-config.sh

# ./01-curl-http_html.test (cs112) 
# ./04-curl-http_html.test (bio)
# ./02-curl-http_bin.test (headshot)
# ./01-curl-https_html (cs112) - CONNECT TUNNEL / SSL


# Test Setup - get all scripts with .test extension
TESTS=$(find /workspaces/Development/https-proxy/proxy/tests -name "*.test" | sort)
TIME_BETWEEN_TESTS=5
# Run all tests
for TEST in ${TESTS}; do
    echo "Running test: ${TEST}"
    ${TEST}
    for i in $(seq 1 ${TIME_BETWEEN_TESTS}); do
        echo -n "."
        sleep 1
    done
    echo ""
done

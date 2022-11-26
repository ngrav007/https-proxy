# Test 10: Test the http-proxy with a simple GET request and response

#!/bin/bash

# Import test config
. $(dirname $0)/config.sh

# Test Configuration
METHOD="GET"
URL="http://www.cs.tufts.edu/comp/112/index.html"
HOST="www.cs.tufts.edu"

# Setup
TESTNAME=$(basename $0)
FETCH_OUT="$REPORT_DIR/$TESTNAME-$FETCH-$OUTPUT_EXT"
CACHE_OUT="$REPORT_DIR/$TESTNAME-$CACHE-$OUTPUT_EXT"
FETCH_REPORT="$REPORT_DIR/$TESTNAME-$FETCH-$REPORT_EXT"
CACHE_REPORT="$REPORT_DIR/$TESTNAME-$CACHE-$REPORT_EXT"

echo "+ ---------- Test 10: Simple GET request and response --------- +"

echo "[*] Sending Request:"
echo "GET http://www.cs.tufts.edu/comp/112/index.html HTTP/1.1"
echo "Host: www.cs.tufts.edu"

echo "[*] Sending initial request to http://www.cs.tufts.edu/comp/112/index.html..."
fetch=$($TIMER $TIMER_FLAGS $FETCH_REPORT $CLIENT $PROXY_HOST $PROXY_PORT $METHOD $HOST $URL 2> $FETCH_OUT)
echo "[*] Sleeping for 5 seconds..."
for i in {1..5}; do
    echo -n "."
    sleep 1
done

echo ""
echo "[*] Sending second request to http://www.cs.tufts.edu/comp/112/index.html..."
cache=$($TIMER $TIMER_FLAGS $CACHE_REPORT $CLIENT $PROXY_HOST $PROXY_PORT $METHOD $HOST $URL 2> $CACHE_OUT)

FETCH_RESULTS=$(cat $FETCH_REPORT)
CACHE_RESULTS=$(cat $CACHE_REPORT)

echo "[+] Results ----------------------------------------------- +"
echo ""
echo "[*] Time Elapsed w/ Fetch: "
echo ""
echo "$FETCH_RESULTS"
echo ""
echo "[*] Time Elapsed w/ Cache: "
echo ""
echo "$CACHE_RESULTS"
echo ""
echo "+ --------------------------------------------------------- +"

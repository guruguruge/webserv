#!/bin/bash

echo "=== Testing basic GET request ==="
echo -e "GET / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n" | nc localhost 8080

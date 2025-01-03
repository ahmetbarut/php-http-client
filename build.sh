#!/bin/bash

# Renklendirme için ANSI kodları
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}PHP HTTP Client Extension Builder${NC}"
echo "================================"

# Extension'ı derle ve kur
echo -e "\n${GREEN}1. Building extension...${NC}"
cd /usr/src/php-http-client && \
phpize && \
./configure && \
make clean && \
make && \
make install

# php.ini'yi güncelle
echo -e "\n${GREEN}2. Updating php.ini...${NC}"
echo 'extension=http_client.so' > /usr/local/etc/php/conf.d/http_client.ini

# Test dosyalarını çalıştır
echo -e "\n${GREEN}3. Running tests...${NC}"
echo -e "\n${YELLOW}Running test.php:${NC}"
php /usr/src/php-http-client/test.php
echo -e "\n${YELLOW}Running test_async.php:${NC}"
php /usr/src/php-http-client/test_async.php

echo -e "\n${GREEN}Build and test completed!${NC}"

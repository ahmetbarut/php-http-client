FROM php:8.2-cli

RUN apt-get update && apt-get install -y \
    libcurl4-openssl-dev \
    pkg-config \
    git \
    zip \
    unzip \
    && docker-php-ext-install curl

WORKDIR /usr/src/php-http-client

CMD ["/bin/bash"]

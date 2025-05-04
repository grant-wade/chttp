#!/bin/bash
wget -O ./vendor/zlib-1.3.1.tar.gz https://zlib.net/zlib-1.3.1.tar.gz
mkdir -p ./vendor/zlib
tar -xzf ./vendor/zlib-1.3.1.tar.gz -C ./vendor/zlib --strip-components=1
cd ./vendor/zlib && ./configure

#!/bin/bash
if [ ! -f private.pem ]; then openssl genrsa -out private.pem -3 3072; fi
if [ ! -f public.pem ] || [ private.pem -nt public.pem ]; then openssl rsa -in private.pem -pubout -out public.pem; fi

#!/usr/bin/bash

docker build -t netsoft.tmit.bme.hu:5050/2024-requiem/measure/rawquic . --progress=plain --no-cache
docker push netsoft.tmit.bme.hu:5050/2024-requiem/measure/rawquic

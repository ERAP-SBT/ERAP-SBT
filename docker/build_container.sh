#!/bin/bash

set +x
set -e

if command -v podman; then
    echo "Using podman"
    podman login gitlab.lrz.de:5005
    podman build --network=host -t gitlab.lrz.de:5005/lrr-tum/students/eragp-sbt-2021/gp_build_env .
    podman push gitlab.lrz.de:5005/lrr-tum/students/eragp-sbt-2021/gp_build_env
else
    echo "Using docker"
    sudo docker login gitlab.lrz.de:5005
    sudo docker build -t gitlab.lrz.de:5005/lrr-tum/students/eragp-sbt-2021/gp_build_env .
    sudo docker push gitlab.lrz.de:5005/lrr-tum/students/eragp-sbt-2021/gp_build_env
fi

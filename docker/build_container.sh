#!/bin/bash

sudo docker login gitlab.lrz.de:5005
sudo docker build -t gitlab.lrz.de:5005/lrr-tum/students/eragp-sbt-2021/gp_build_env .
sudo docker push gitlab.lrz.de:5005/lrr-tum/students/eragp-sbt-2021/gp_build_env

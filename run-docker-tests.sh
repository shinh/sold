#! /bin/bash -eux

sudo docker build -f ubuntu18.04.Dockerfile .
sudo docker build -f ubuntu20.04.Dockerfile .
sudo docker build -f ubuntu22.04.Dockerfile .
sudo docker build -f fedora-latest.Dockerfile .

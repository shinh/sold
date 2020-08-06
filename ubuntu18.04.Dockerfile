FROM ubuntu:18.04
COPY . /sold
WORKDIR /sold
RUN apt-get update
RUN apt-get install -y ninja-build cmake gcc g++
RUN rm -rf build
RUN mkdir build && cd build && cmake -GNinja ..
RUN cmake --build build
RUN cd tests && ./run-all-tests.sh

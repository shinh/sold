FROM ubuntu:20.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update
RUN apt-get install -y ninja-build cmake gcc g++ git libdwarf-dev
COPY . /sold
WORKDIR /sold
RUN rm -rf build
RUN mkdir build && cd build && cmake -GNinja ..
RUN cmake --build build
RUN cd tests && ./run-all-tests.sh

FROM ubuntu:20.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update
RUN apt-get install -y ninja-build cmake gcc g++ git python3 python3-distutils python3-dev python3-pip
RUN pip3 install pytest
COPY . /sold
WORKDIR /sold
RUN rm -rf build
RUN mkdir build && cd build && cmake -GNinja -DSOLD_PYBIND_TEST=ON ..
RUN cmake --build build
RUN cd build && ctest

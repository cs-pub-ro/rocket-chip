FROM ubuntu:24.04

# Install dependencies
RUN apt-get update \
  && apt-get install -y git autoconf automake autotools-dev curl libmpc-dev libmpfr-dev libgmp-dev libusb-1.0-0-dev gawk build-essential bison flex texinfo gperf libtool patchutils bc zlib1g-dev device-tree-compiler pkg-config libexpat-dev libfl-dev wget gpg apt-transport-https openjdk-17-jdk verilator cmake ninja-build clang-19 llvm-19 lld-19 \
  && rm -rf /var/lib/apt/lists/*

# Install Mill, Firtool, and Rocket Chip tools (riscv)
RUN curl -q -L https://github.com/com-lihaoyi/mill/releases/download/0.11.7/0.11.7 -o /usr/local/bin/mill && chmod +x /usr/local/bin/mill \
  && wget -q -O - https://github.com/llvm/circt/releases/download/firtool-1.56.1/circt-full-shared-linux-x64.tar.gz | tar -zx \
  && wget -q -O - https://github.com/chipsalliance/rocket-tools/releases/download/2023.01.30/rocket-tools-ubuntu-22.04-nightly-2023.01.30-nightly.tar.gz | tar -zx && mv /riscv /rocket-tools-riscv

ENV PATH="$PATH:/firtool-1.56.1/bin"
ENV RISCV="/rocket-tools-riscv"
ENV JAVA_HOME="/usr/lib/jvm/java-17-openjdk-amd64"

ENTRYPOINT ["/bin/bash"]


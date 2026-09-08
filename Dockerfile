FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

RUN apt-get update && apt-get install -y --no-install-recommends \
    bash \
    ca-certificates \
    build-essential \
    clang \
    flex \
    bison \
    g++ \
    gawk \
    gcc-multilib \
    g++-multilib \
    gcc-9 \
    g++-9 \
    gcc-9-multilib \
    g++-9-multilib \
    gettext \
    git \
    ccache \
    libelf-dev \
    libncurses-dev \
    libssl-dev \
    python3 \
    python3-distutils \
    python3-setuptools \
    rsync \
    subversion \
    swig \
    unzip \
    zlib1g-dev \
    file \
    wget \
    perl \
    quilt \
    xz-utils \
    zstd \
    time \
    xsltproc \
    && rm -rf /var/lib/apt/lists/*

RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 90 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-9 90 \
    && update-alternatives --install /usr/bin/cc cc /usr/bin/gcc-9 90 \
    && update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++-9 90 \
    && update-alternatives --set gcc /usr/bin/gcc-9 \
    && update-alternatives --set g++ /usr/bin/g++-9 \
    && update-alternatives --set cc /usr/bin/gcc-9 \
    && update-alternatives --set c++ /usr/bin/g++-9 \
    && ln -sf /usr/bin/bash /bin/sh

ARG BUILD_USER=builder
ARG BUILD_UID=1000
ARG BUILD_GID=1000

RUN groupadd --gid "${BUILD_GID}" "${BUILD_USER}" \
    && useradd --uid "${BUILD_UID}" --gid "${BUILD_GID}" \
        --create-home --shell /bin/bash "${BUILD_USER}"

USER ${BUILD_USER}
WORKDIR /workspace

RUN gcc --version | sed -n '1p' \
    && g++ --version | sed -n '1p' \
    && make --version | sed -n '1p' \
    && python3 --version

CMD ["/bin/bash"]

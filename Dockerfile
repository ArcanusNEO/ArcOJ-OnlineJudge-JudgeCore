FROM almalinux:9

RUN dnf update -y && \
    dnf group install -y "Development Tools" && dnf install -y glibc-devel \
    # gcc and static lib
    dnf install -y python && \
    # python
    dnf install -y nodejs && \
    # nodejs
    dnf install -y golang
    # go

RUN cd /tmp && \
    mkdir -p /usr/local/libexec/pypy3 && \
    curl -fsSLO https://downloads.python.org/pypy/pypy3.9-v7.3.11-linux64.tar.bz2 && \
    tar -xavf pypy3.9-v7.3.11-linux64.tar.bz2 --strip-components=1 -C /usr/local/libexec/pypy3 && \
    ln -srf /usr/local/libexec/pypy3/bin/pypy* /usr/local/bin && \
    ln -srf /usr/local/libexec/pypy3/bin/libpypy*.so* /usr/local/lib64
    # pypy3

WORKDIR /usr/bin/judgecore
ADD judgecore /usr/bin/judgecore
RUN set -ex; \
    source /usr/bin/judgecore/compile.sh;

# volume

VOLUME /mnt/data

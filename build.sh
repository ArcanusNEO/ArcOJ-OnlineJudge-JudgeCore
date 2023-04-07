#!/bin/bash

MOUNTDIR="/var/www/data"
MOUNTPOINT="/var/www/data"

# if a warning about SIGNAL occurred on build, use this:
# dpkg -r --force-depends golang-docker-credential-helpers
# this is a bug related to docker-compose

set -ex

docker build -t arcoj/judgecore:0.2.1 .

docker run --restart=always --name=judgecore --cap-add=SYS_PTRACE --cap-add=SYS_ADMIN --security-opt='apparmor=unconfined' --security-opt='seccomp=unconfined' -itd -v "${MOUNTDIR}":"${MOUNTPOINT}" --tmpfs='/tmp:exec' --ulimit='stack=-1' arcoj/judgecore:0.2.1

# echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope

#!/bin/sh
set -e

UNAME=$1

if command -v apt-get >/dev/null
then
  apt-get update
  apt-get install -y sudo
  apt-get clean
elif command -v dnf >/dev/null
then
  dnf install -y sudo
  dnf clean all
elif command -v yum >/dev/null
then
  yum install -y sudo
  yum clean all
fi

[ $(id -u sensei) ] || useradd -m -s /bin/bash $UNAME
echo "$UNAME ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers.d/$UNAME

#!/bin/bash

set -ex

MAINLINE_KERNEL_VERSION=6.8
UBUNTU_VERSION=24.04
UBUNTU_CODENAME=noble
UBUNTU_KERNEL_BRANCH=master

for arg in "$@"; do
  shift
  case "$arg" in
    '--help')   HELP=true   ;;
    '--auto')   AUTO=true   ;;
  esac
done

if [[ ${HELP} ]]; then
    echo "Usage: $0 [--help] [--auto]"
    exit
fi


echo "===Starting debian directory update.==="

SCRIPT=$(realpath "$0")
SCRIPTPATH=$(dirname "${SCRIPT}")
cd "${SCRIPTPATH}"


echo "===Copy configuration.==="

TUXEDO_KERNEL_BRANCH=tuxedo-${MAINLINE_KERNEL_VERSION}

cp --force debian.${UBUNTU_KERNEL_BRANCH}/dkms-versions debian.${TUXEDO_KERNEL_BRANCH}/dkms-versions
cp --force debian.${UBUNTU_KERNEL_BRANCH}/config/README.rst debian.${TUXEDO_KERNEL_BRANCH}/config/README.rst
cp --force debian.${UBUNTU_KERNEL_BRANCH}/config/annotations debian.${TUXEDO_KERNEL_BRANCH}/config/annotations
cp --force debian.${UBUNTU_KERNEL_BRANCH}/control.stub.in debian.${TUXEDO_KERNEL_BRANCH}/control.stub.in
cp --force debian.${UBUNTU_KERNEL_BRANCH}/control.d/generic.inclusion-list debian.${TUXEDO_KERNEL_BRANCH}/control.d/generic.inclusion-list
cp --force debian.${UBUNTU_KERNEL_BRANCH}/control.d/flavour-control.stub debian.${TUXEDO_KERNEL_BRANCH}/control.d/flavour-control.stub
cp --force debian.${UBUNTU_KERNEL_BRANCH}/control.d/vars.generic debian.${TUXEDO_KERNEL_BRANCH}/control.d/vars.tuxedo
cp --force debian.${UBUNTU_KERNEL_BRANCH}/modprobe.d/common.conf debian.${TUXEDO_KERNEL_BRANCH}/modprobe.d/common.conf
cp --force debian.${UBUNTU_KERNEL_BRANCH}/rules.d/amd64.mk debian.${TUXEDO_KERNEL_BRANCH}/rules.d/amd64.mk


echo "===Edit configuration.==="

awk --include inplace '{count+=gsub("^# ARCH: amd64 arm64 armhf ppc64el riscv64 s390x$","# ARCH: amd64");print}END{if(count!=1)exit 1}' debian.${TUXEDO_KERNEL_BRANCH}/config/annotations
awk --include inplace '{count+=gsub("^# FLAVOUR: amd64-generic arm64-generic arm64-generic-64k armhf-generic ppc64el-generic riscv64-generic s390x-generic$","# FLAVOUR: amd64-tuxedo");print}END{if(count!=1)exit 1}' debian.${TUXEDO_KERNEL_BRANCH}/config/annotations
awk --include inplace '{count+=gsub("^Maintainer: Ubuntu Kernel Team <kernel-team@lists.ubuntu.com>$","Maintainer: TUXEDO Computers GmbH <tux@tuxedocomputers.com>");print}END{if(count!=1)exit 1}'  debian.${TUXEDO_KERNEL_BRANCH}/control.stub.in
awk --include inplace '{count+=gsub("^Vcs-Git: git://git.launchpad.net/~ubuntu-kernel/ubuntu/\\+source/linux/\\+git/=SERIES=$","Vcs-Git: https://gitlab.com/tuxedocomputers/development/packages/linux.git");print}END{if(count!=1)exit 1}' debian.${TUXEDO_KERNEL_BRANCH}/control.stub.in
awk --include inplace '{count+=gsub("^arch=\"amd64 armhf arm64 ppc64el s390x\"$","arch=\"amd64\"");print}END{if(count!=1)exit 1}' debian.${TUXEDO_KERNEL_BRANCH}/control.d/vars.tuxedo
awk --include inplace '{count+=gsub("^supported=\"Generic\"$","supported=\"Tuxedo\"");print}END{if(count!=1)exit 1}' debian.${TUXEDO_KERNEL_BRANCH}/control.d/vars.tuxedo
awk --include inplace '{count+=gsub("^flavours	= generic$","flavours	= tuxedo");print}END{if(count!=1)exit 1}' debian.${TUXEDO_KERNEL_BRANCH}/rules.d/amd64.mk

LANG=C fakeroot debian/rules clean
LANG=C fakeroot debian/rules updateconfigs || true
LANG=C fakeroot debian/rules updateconfigs


echo "===Done.==="

if [[ ${AUTO} ]]; then
    ./03-tuxedo-update-version.sh
else
    echo "Run ./03-tuxedo-update-version.sh next to update changelog and tag."
fi

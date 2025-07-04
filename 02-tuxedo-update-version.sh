#!/bin/bash

set -e

MAINLINE_KERNEL_VERSION=6.19
DEBIAN_CODENAME=forky
DEBIAN_KERNEL_BRANCH=debian/${MAINLINE_KERNEL_VERSION}/${DEBIAN_CODENAME}

for arg in "$@"; do
  shift
  case "$arg" in
    '--help')   HELP=true   ;;
    '--dry')    DRY=true    ;;
  esac
done

if [[ ${HELP} ]]; then
    echo "Usage: $0 [--help] [--dry] [--abi|--build]"
    exit
fi


echo "===Starting version update.==="

SCRIPT=$(realpath "$0")
SCRIPTPATH=$(dirname "${SCRIPT}")
cd "${SCRIPTPATH}"


echo "===Gather version informations.==="

TUXEDO_VERSION=$(grep --perl-regexp --only-matching --max-count=1 "(?<=^linux \().*(?=\) tuxedo)" debian/changelog || echo 0.0.0-0tux0)
TUXEDO_ABI=$((${TUXEDO_VERSION##*tux}+1))

BASE_VERSION=$(grep --perl-regexp --only-matching --max-count=1 "(?<=^linux \().*(?=\) unstable)" debian/changelog)


echo "===Update changelog and commit.==="

if [[ ${DRY} ]]; then
    echo "Dry run. Would execute:"
    echo "    DEBFULLNAME=\"Tuxedo BOT\" DEBEMAIL=\"tux@tuxedocomputers.com\" gbp dch --new-version=\"${BASE_VERSION}tux${TUXEDO_ABI}\" --distribution=tuxedo --force-distribution --release --ignore-branch --spawn-editor=never"
    echo "    git commit --signoff --message=\"TUXEDO: ${BASE_VERSION}tux${TUXEDO_ABI}\" --message=\"Gbp-Dch: ignore\" debian/changelog"
    echo "    git tag --sign --message=\"debian/${BASE_VERSION}tux${TUXEDO_ABI}\" \"debian/${BASE_VERSION}tux${TUXEDO_ABI}\""
else
    DEBFULLNAME="Tuxedo BOT" DEBEMAIL="tux@tuxedocomputers.com" gbp dch --new-version="${BASE_VERSION}tux${TUXEDO_ABI}" --distribution=tuxedo --force-distribution --release --ignore-branch --spawn-editor=never
    git commit --signoff --message="TUXEDO: ${BASE_VERSION}tux${TUXEDO_ABI}" --message="Gbp-Dch: ignore" debian/changelog
    git tag --sign --message="debian/${BASE_VERSION}tux${TUXEDO_ABI}" "debian/${BASE_VERSION}tux${TUXEDO_ABI}"
fi

echo "===Done.==="

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
    '--dry')    DRY=true    ;;
    '--auto')   AUTO=true   ;;
  esac
done

if [[ ${HELP} ]]; then
    echo "Usage: $0 [--help] [--dry] [--auto]"
    exit
fi


echo "===Starting repository update.==="

SCRIPT=$(realpath "$0")
SCRIPTPATH=$(dirname "${SCRIPT}")
cd "${SCRIPTPATH}"


echo "===Fetching newest tags from upstream.==="

UBUNTU_KERNEL_BRANCH_NEXT=${UBUNTU_KERNEL_BRANCH}-next

if git remote | grep ubuntu-${UBUNTU_CODENAME}; then
    git remote set-url ubuntu-${UBUNTU_CODENAME} git://git.launchpad.net/~ubuntu-kernel/ubuntu/+source/linux/+git/${UBUNTU_CODENAME}
else
    git remote add ubuntu-${UBUNTU_CODENAME} git://git.launchpad.net/~ubuntu-kernel/ubuntu/+source/linux/+git/${UBUNTU_CODENAME}
fi
git fetch ubuntu-${UBUNTU_CODENAME} --tags


echo "===Check newest version from upstream.==="

UBUNTU_MAINLINE_VERSION=${MAINLINE_KERNEL_VERSION}.0
ESCAPED_UBUNTU_MAINLINE_VERSION=${MAINLINE_KERNEL_VERSION%%.*}\\.${MAINLINE_KERNEL_VERSION##*.}\\.0

if [[ "${UBUNTU_KERNEL_BRANCH}" == "master" ]]; then
    CURRENT_BASE_TAG=$(git describe --tags --match "Ubuntu-${ESCAPED_UBUNTU_MAINLINE_VERSION}-*" --abbrev=0)
else
    CURRENT_BASE_TAG=$(git describe --tags --match "Ubuntu-${UBUNTU_KERNEL_BRANCH}-${ESCAPED_UBUNTU_MAINLINE_VERSION}-*" --abbrev=0)
fi
CURRENT_BASE_TAG_NUMBER=${UBUNTU_MAINLINE_VERSION}-${CURRENT_BASE_TAG##*-${ESCAPED_UBUNTU_MAINLINE_VERSION}-}
CURRENT_BASE_VERSION_NUMBER=${CURRENT_BASE_TAG_NUMBER//_/\~}

NEWEST_BASE_TAG=$(git describe --tags ubuntu-${UBUNTU_CODENAME}/${UBUNTU_KERNEL_BRANCH_NEXT} --abbrev=0)
NEWEST_BASE_TAG_NUMBER=${UBUNTU_MAINLINE_VERSION}-${NEWEST_BASE_TAG##*-${ESCAPED_UBUNTU_MAINLINE_VERSION}-}
NEWEST_BASE_VERSION_NUMBER=${NEWEST_BASE_TAG_NUMBER//_/\~}

if [[ ${CURRENT_BASE_VERSION_NUMBER} == ${NEWEST_BASE_VERSION_NUMBER} ]]; then
    echo "===Version already up to date. Exiting.==="
    exit
elif [[ $(git diff --name-status --diff-filter=AD ${CURRENT_BASE_TAG} ${NEWEST_BASE_TAG}) ]]; then
    echo "===Structural changes to abstracted debian directory detected. Update scripts need to be adapted. Exiting.==="
    exit 1
fi


echo "===Rebase onto newest upstream version.==="

if [[ ${DRY} ]]; then
    echo "Dry run. Would execute: git rebase ${CURRENT_BASE_TAG} --onto ${NEWEST_BASE_TAG}"
    exit
else
    git rebase ${CURRENT_BASE_TAG} --onto ${NEWEST_BASE_TAG}
fi


echo "===Done.==="

if [[ ${AUTO} ]]; then
    ./02-tuxedo-update-debian-dir.sh --auto
else
    echo "Run ./02-tuxedo-update-debian-dir.sh next to update abstracted debian directory."
fi

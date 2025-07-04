#!/bin/bash

set -e

MAINLINE_KERNEL_VERSION=6.19
DEBIAN_CODENAME=forky
DEBIAN_KERNEL_BRANCH=debian/${MAINLINE_KERNEL_VERSION}/${DEBIAN_CODENAME}

for arg in "${@}"; do
  shift
  case "${arg}" in
    "--help")   HELP=true   ;;
    "--dry")    DRY=true    ;;
    "--auto")   AUTO=true   ;;
  esac
done

if [[ ${HELP} ]]; then
    echo "Usage: ${0} [--help] [--dry] [--auto]"
    exit
fi


echo "===Starting repository update.==="

SCRIPT=$(realpath "${0}")
SCRIPTPATH=$(dirname "${SCRIPT}")
cd "${SCRIPTPATH}"


echo "===Fetching newest tags from upstream.==="

if git remote | grep debian; then
    git remote set-url debian https://salsa.debian.org/kernel-team/linux.git
else
    git remote add debian https://salsa.debian.org/kernel-team/linux.git
fi
git fetch debian ${DEBIAN_KERNEL_BRANCH} --tags --prune --force


echo "===Check newest version from upstream.==="

CURRENT_BASE_TAG=$(git describe --tags --abbrev=0 --match="debian/*" --exclude "debian/*tux*")
CURRENT_BASE_TAG_NUMBER=${CURRENT_BASE_TAG#"debian/"}

NEWEST_BASE_TAG=$(git describe --tags --abbrev=0 --match="debian/*" debian/"${DEBIAN_KERNEL_BRANCH}")
NEWEST_BASE_TAG_NUMBER=${NEWEST_BASE_TAG#"debian/"}

if [[ ${CURRENT_BASE_TAG_NUMBER} == "${NEWEST_BASE_TAG_NUMBER}" ]]; then
    echo "===Version already up to date. Exiting.==="
    exit
fi


echo "===Rebase onto newest upstream version.==="

if [[ ${DRY} ]]; then
    echo "Dry run. Would execute:"
    echo "    git filter-repo --invert-paths --path=debian/changelog --refs=\"${CURRENT_BASE_TAG}\"..HEAD --force"
    echo "    git rebase \"${CURRENT_BASE_TAG}\" --onto=\"${NEWEST_BASE_TAG}\""
    exit
else
    git filter-repo --invert-paths --path=debian/changelog --refs="${CURRENT_BASE_TAG}"..HEAD --force
    git rebase "${CURRENT_BASE_TAG}" --onto="${NEWEST_BASE_TAG}"
fi


echo "===Done.==="

if [[ ${AUTO} ]]; then
    ./02-tuxedo-update-version.sh
else
    echo "Run ./02-tuxedo-update-version.sh next to update changelog and tag."
fi

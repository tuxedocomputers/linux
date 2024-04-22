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
    '--build')  BUILD=true  ;;
  esac
done

if [[ ${HELP} ]]; then
    echo "Usage: $0 [--help] [--dry] [--build]"
    exit
fi


echo "===Starting version update.==="

SCRIPT=$(realpath "$0")
SCRIPTPATH=$(dirname "${SCRIPT}")
cd "${SCRIPTPATH}"


echo "===Fetching newest tags from upstream.==="

TUXEDO_KERNEL_BRANCH=tuxedo-${MAINLINE_KERNEL_VERSION}
TUXEDO_KERNEL_BRANCH_FULL=${TUXEDO_KERNEL_BRANCH}-${UBUNTU_VERSION}

if git remote | grep tuxedo; then
    git remote set-url tuxedo https://gitlab.com/tuxedocomputers/development/packages/linux.git
else
    git remote add tuxedo https://gitlab.com/tuxedocomputers/development/packages/linux.git
fi
git fetch tuxedo ${TUXEDO_KERNEL_BRANCH_FULL} --tags --prune --force


echo "===Gather version informations.==="

UBUNTU_MAINLINE_VERSION=${MAINLINE_KERNEL_VERSION}.0
ESCAPED_UBUNTU_MAINLINE_VERSION=${MAINLINE_KERNEL_VERSION%%.*}\\.${MAINLINE_KERNEL_VERSION##*.}\\.0
ESCAPED_TUXEDO_KERNEL_BRANCH=tuxedo-${MAINLINE_KERNEL_VERSION%%.*}\\.${MAINLINE_KERNEL_VERSION##*.}


NEWEST_TUXEDO_TAG=$(git describe tuxedo/${TUXEDO_KERNEL_BRANCH_FULL} --tags --match "Ubuntu-${ESCAPED_TUXEDO_KERNEL_BRANCH}-*" --abbrev=0)

NEWEST_TUXEDO_ABI_AND_BUILD=${NEWEST_TUXEDO_TAG#Ubuntu-${ESCAPED_TUXEDO_KERNEL_BRANCH}-${ESCAPED_UBUNTU_MAINLINE_VERSION}-}
NEWEST_TUXEDO_ABI=${NEWEST_TUXEDO_ABI_AND_BUILD%%.*}
NEWEST_TUXEDO_ABI_NUMBER=$((${NEWEST_TUXEDO_ABI} / 1000))


CURRENT_TUXEDO_VERSION_NUMBER=($(grep -Pom1 "(?<=^linux-${TUXEDO_KERNEL_BRANCH} \().*(?=\))" debian.${TUXEDO_KERNEL_BRANCH}/changelog))

CURRENT_TUXEDO_ABI_AND_BUILD=${CURRENT_TUXEDO_VERSION_NUMBER#${ESCAPED_UBUNTU_MAINLINE_VERSION}-}
CURRENT_TUXEDO_ABI=${CURRENT_TUXEDO_ABI_AND_BUILD%%.*}
CURRENT_TUXEDO_ABI_NUMBER=$((${CURRENT_TUXEDO_ABI} / 1000))

CURRENT_TUXEDO_BUILD_NUMBER=${CURRENT_TUXEDO_VERSION_NUMBER##*tux}

CURRENT_TUXEDO_TAG_NUMBER=${CURRENT_TUXEDO_VERSION_NUMBER//\~/_}
CURRENT_TUXEDO_TAG=Ubuntu-${TUXEDO_KERNEL_BRANCH}-${CURRENT_TUXEDO_TAG_NUMBER}


if [[ "${UBUNTU_KERNEL_BRANCH}" == "master" ]]; then
    NEXT_BASE_TAG=$(git describe --tags --match "Ubuntu-${ESCAPED_UBUNTU_MAINLINE_VERSION}-*" --abbrev=0)
else
    NEXT_BASE_TAG=$(git describe --tags --match "Ubuntu-${UBUNTU_KERNEL_BRANCH}-${ESCAPED_UBUNTU_MAINLINE_VERSION}-*" --abbrev=0)
fi
NEXT_BASE_TAG_NUMBER=${UBUNTU_MAINLINE_VERSION}-${NEXT_BASE_TAG##*-${ESCAPED_UBUNTU_MAINLINE_VERSION}-}
NEXT_BASE_VERSION_NUMBER=${NEXT_BASE_TAG_NUMBER//_/\~}
NEXT_BASE_ABI_AND_BUILD=${NEXT_BASE_VERSION_NUMBER#${ESCAPED_UBUNTU_MAINLINE_VERSION}-}
NEXT_BASE_ABI_NUMBER=${NEXT_BASE_ABI_AND_BUILD%%.*}
NEXT_BASE_BUILD=${NEXT_BASE_ABI_AND_BUILD#*.}


if [[ ${BUILD} ]]; then
    NEXT_TUXEDO_ABI_NUMBER=${CURRENT_TUXEDO_ABI_NUMBER}
    NEXT_TUXEDO_BUILD_NUMBER=$((${CURRENT_TUXEDO_BUILD_NUMBER} + 1))
else
    NEXT_TUXEDO_ABI_NUMBER=$((${NEWEST_TUXEDO_ABI_NUMBER} + 1))
    NEXT_TUXEDO_BUILD_NUMBER=1
fi
NEXT_TUXEDO_VERSION_NUMBER=${UBUNTU_MAINLINE_VERSION}-$((${NEXT_TUXEDO_ABI_NUMBER} * 1000 + NEXT_BASE_ABI_NUMBER)).${NEXT_BASE_BUILD}tux${NEXT_TUXEDO_BUILD_NUMBER}
NEXT_TUXEDO_TAG_NUMBER=${NEXT_TUXEDO_VERSION_NUMBER//\~/_}
NEXT_TUXEDO_TAG=Ubuntu-${TUXEDO_KERNEL_BRANCH}-${NEXT_TUXEDO_TAG_NUMBER}


echo "===Update changelog.==="

if [[ ${DRY} ]]; then
    echo "Dry run. Would execute:"
    echo "    cp debian.${UBUNTU_KERNEL_BRANCH}/changelog debian/changelog"
    echo "    gbp dch --new-version=${NEXT_TUXEDO_VERSION_NUMBER} --release --since ${NEXT_BASE_TAG} --ignore-branch --spawn-editor=never"
    echo "    if [[ \"$UBUNTU_KERNEL_BRANCH\" == \"master\" ]]; then"
    echo "        awk --include inplace \"NR!=1{print}NR==1{count+=gsub(\\\"^linux \\\",\\\"linux-${TUXEDO_KERNEL_BRANCH} \\\");print}END{if(count!=1)exit 1}\" debian/changelog"
    echo "    else"
    echo "        awk --include inplace \"NR!=1{print}NR==1{count+=gsub(\\\"^linux-${UBUNTU_KERNEL_BRANCH} \\\",\\\"linux-${TUXEDO_KERNEL_BRANCH} \\\");print}END{if(count!=1)exit 1}\" debian/changelog"
    echo "    fi"
    echo "    mv debian/changelog debian.${TUXEDO_KERNEL_BRANCH}/changelog"
else
    cp debian.${UBUNTU_KERNEL_BRANCH}/changelog debian/changelog
    gbp dch --new-version=${NEXT_TUXEDO_VERSION_NUMBER} --release --since ${NEXT_BASE_TAG} --ignore-branch --spawn-editor=never
    if [[ "$UBUNTU_KERNEL_BRANCH" == "master" ]]; then
        awk --include inplace "NR!=1{print}NR==1{count+=gsub(\"^linux \",\"linux-${TUXEDO_KERNEL_BRANCH} \");print}END{if(count!=1)exit 1}" debian/changelog
    else
        awk --include inplace "NR!=1{print}NR==1{count+=gsub(\"^linux-${UBUNTU_KERNEL_BRANCH} \",\"linux-${TUXEDO_KERNEL_BRANCH} \");print}END{if(count!=1)exit 1}" debian/changelog
    fi
    mv debian/changelog debian.${TUXEDO_KERNEL_BRANCH}/changelog
fi


echo "===Commit and tag.==="

if [[ ${DRY} ]]; then
    echo "Dry run. Would execute:"
    echo "    git add debian.${TUXEDO_KERNEL_BRANCH}"
    echo "    git commit --signoff --message \"TUXEDO: ${NEXT_TUXEDO_TAG//_/\~}\" --message \"Gbp-Dch: ignore\""
    echo "    git tag --sign --message \"${NEXT_TUXEDO_TAG//_/\~}\" ${NEXT_TUXEDO_TAG}"
else
    git add debian.${TUXEDO_KERNEL_BRANCH}
    git commit --signoff --message "TUXEDO: ${NEXT_TUXEDO_TAG//_/\~}" --message "Gbp-Dch: ignore"
    git tag --sign --message "${NEXT_TUXEDO_TAG//_/\~}" ${NEXT_TUXEDO_TAG}
fi

echo "===Done.==="

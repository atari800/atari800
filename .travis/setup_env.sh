#!/bin/sh
# Use as: ". setup_env.sh"

export GITHUB_USER=$(echo "${TRAVIS_REPO_SLUG}" | cut -d '/' -f 1)
export BASE_RAW_URL="https://raw.githubusercontent.com/${GITHUB_USER}"
export PROJECT=$(echo "${TRAVIS_REPO_SLUG}" | cut -d '/' -f 2)
export SHORT_ID=$(git log -n1 --format="%h")
export PROJECT_LOWER=`echo ${PROJECT} | tr '[[:upper:]]' '[[:lower:]]'`

CPU=unknown
if echo "" | gcc -dM  -E - | grep -q __x86_64__; then
	CPU=x86_64
fi
if echo "" | gcc -dM  -E - | grep -q __i386__; then
	CPU=i386
fi
if echo "" | gcc -dM  -E - | grep -q "__arm.*__"; then
	CPU=arm
fi
export CPU

case "$TRAVIS_OS_NAME" in
linux)
	archive_tag=-linux
	;;

osx)
	archive_tag=-macosx
	;;

esac

export archive_tag

#!/bin/sh

# This script deploys the built binaries to bintray:
# https://bintray.com/atari800/atari800-files

# Bintray needs an api key for access as password.
# This must have been set as environment variable BINTRAY_API_KEY
# in the settings page of your travis account.
# You will find the key in your profile on bintray.

if [ -z "$BINTRAY_API_KEY" ]
then
	echo "error: BINTRAY_API_KEY is undefined" >&2
	exit 1
fi

OUT="${PWD}/.travis/out"

# variables
RELEASE_DATE=`date -u +%Y-%m-%dT%H:%M:%S`
BINTRAY_HOST=https://api.bintray.com
BINTRAY_USER="${BINTRAY_USER:-th-otto}"
BINTRAY_REPO_OWNER="${BINTRAY_REPO_OWNER:-$BINTRAY_USER}" # owner and user not always the same
BINTRAY_REPO="${BINTRAY_REPO_OWNER}/${BINTRAY_REPO:-atari800-files}"

if [ "${TRAVIS_PULL_REQUEST}" != "false" ]
then
	BINTRAY_DIR=pullrequests
	BINTRAY_DESC="[${TRAVIS_REPO_SLUG}] Download: https://dl.bintray.com/${BINTRAY_REPO}/${BINTRAY_DIR}/${ARCHIVE}"
else
	BINTRAY_DIR=snapshots
	BINTRAY_DESC="[${PROJECT}] [${TRAVIS_BRANCH}] Commit: https://github.com/${GITHUB_USER}/${PROJECT}/commit/${TRAVIS_COMMIT}"
fi

# use the commit id as 'version' for bintray
BINTRAY_VERSION=$TRAVIS_COMMIT

echo "Deploying $ARCHIVE to ${BINTRAY_HOST}/${BINTRAY_REPO}"
echo "See result at ${BINTRAY_HOST}/${BINTRAY_REPO}/${BINTRAY_DIR}#files"

# See https://bintray.com/docs/api for a description of the REST API

CURL="curl -u ${BINTRAY_USER}:${BINTRAY_API_KEY} -H Accept:application/json -w \n"

cd "$OUT"

#create version:
echo "creating version ${BINTRAY_DIR}/${BINTRAY_VERSION}"
$CURL --data '{"name":"'"${BINTRAY_VERSION}"'","released":"'"${RELEASE_DATE}"'","desc":"'"${BINTRAY_DESC}"'","published":true}' --header 'Content-Type: application/json' "${BINTRAY_HOST}/packages/${BINTRAY_REPO}/${BINTRAY_DIR}/versions"
echo ""

#upload file:
echo "upload ${BINTRAY_DIR}/${ARCHIVE}"
$CURL --upload "${ARCHIVE}" "${BINTRAY_HOST}/content/${BINTRAY_REPO}/${BINTRAY_DIR}/${BINTRAY_VERSION}/${BINTRAY_DIR}/${ARCHIVE}?publish=1&override=1&explode=0"
echo ""

# publish the version
echo "publish ${BINTRAY_DIR}/${BINTRAY_VERSION}"
$CURL --data '' "${BINTRAY_HOST}/content/${BINTRAY_REPO}/${BINTRAY_DIR}/${BINTRAY_VERSION}/publish?publish_wait_for_secs=-1"
echo ""


if $isrelease; then

	echo "celebrating new release ${VERSION}"
	BINTRAY_DIR=releases
	BINTRAY_VERSION=$VERSION

#create version:
	echo "creating version ${BINTRAY_DIR}/${BINTRAY_VERSION}"
	$CURL --data '{"name":"'"${BINTRAY_VERSION}"'","released":"'"${RELEASE_DATE}"'","desc":"'"${BINTRAY_DESC}"'","published":true}' --header 'Content-Type: application/json' "${BINTRAY_HOST}/packages/${BINTRAY_REPO}/${BINTRAY_DIR}/versions"
	echo ""
	
#upload file(s):
	# we only need to upload the src archive once
	if test "$TRAVIS_OS_NAME" = linux; then
		for ext in gz bz2 xz lz; do
			SRCARCHIVE="${PROJECT_LOWER}-${VERSION}.tar.${ext}"
			if test -f "${SRCARCHIVE}"; then
				echo "upload ${BINTRAY_DIR}/${VERSION}/${SRCARCHIVE}"
				$CURL --upload "${SRCARCHIVE}" "${BINTRAY_HOST}/content/${BINTRAY_REPO}/${BINTRAY_DIR}/${BINTRAY_VERSION}/${VERSION}/${PROJECT_LOWER}-${VERSION}.orig.tar.${ext}?publish=1&override=0&explode=0"
				echo ""
			fi
		done
	fi
	echo "upload ${BINTRAY_DIR}/${ARCHIVE}"
	$CURL --upload "${ARCHIVE}" "${BINTRAY_HOST}/content/${BINTRAY_REPO}/${BINTRAY_DIR}/${BINTRAY_VERSION}/${VERSION}/${ARCHIVE}?publish=1?override=0?explode=0"
	
# publish the version
	echo "publish ${BINTRAY_DIR}/${BINTRAY_VERSION}"
	$CURL --data '' "${BINTRAY_HOST}/content/${BINTRAY_REPO}/${BINTRAY_DIR}/${BINTRAY_VERSION}/publish?publish_wait_for_secs=-1"
	echo ""

fi

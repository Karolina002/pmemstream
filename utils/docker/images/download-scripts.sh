#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2022, Intel Corporation

#
# download-scripts.sh - downloads latest version of
#			codecov's uploader to generate and upload reports.

set -e

if [ "${SKIP_SCRIPTS_DOWNLOAD}" ]; then
	echo "Variable 'SKIP_SCRIPTS_DOWNLOAD' is set; skipping scripts' download"
	exit
fi

mkdir -p /opt/scripts

if ! [ -x "$(command -v curl)" ]; then
	echo "Error: curl is not installed."
	return 1
fi

# Download codecov and check integrity
mkdir -p codecov-tmp
cd codecov-tmp

git clone https://github.com/Karolina002/uploader
cd uploader

if ! [ -x "$(command -v npm)" ]; then
	echo "Error: npm is not installed."
	return 1
fi

npm install
npm run build
npm run build-linux

cd dist/bin/

chmod +x codecov.js

mv -v codecov.js /opt/scripts/codecov

cd ../../../../
rm -rf codecov-tmp

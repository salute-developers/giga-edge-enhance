#!/usr/bin/env sh

set -ex

REPO_ROOT="$(dirname "$0")/../.."
APP_DIR="$REPO_ROOT/apps/spench-site"
cd "$REPO_ROOT/packages/web-sdk"

npm pack
mv *.tgz "$APP_DIR/web-sdk.tgz"

cd "$APP_DIR"

npm i --no-save ./web-sdk.tgz

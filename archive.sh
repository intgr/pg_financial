#!/bin/sh
set -e

if [[ -z "$1" ]]; then
  TAG="$(git describe)"
  VER="${TAG#v}"
else
  VER="$1"
  TAG="v$VER"
fi

NAME="pg_financial-$VER"
echo "Creating `realpath ../$NAME.tar.gz`"
git archive "$TAG" --prefix "$NAME/" -o "../$NAME.tar.gz"
echo "Creating `realpath ../$NAME.zip`"
git archive "$TAG" --prefix "$NAME/" -o "../$NAME.zip"

#!/bin/bash
SRCDIR=$1
pushd $SRCDIR > /dev/null
SHA=$(git rev-list -1 HEAD)
REMOTE=$(git remote get-url origin)
popd > /dev/null
echo git clone ${REMOTE} llvm
echo cd llvm
echo git checkout ${SHA}

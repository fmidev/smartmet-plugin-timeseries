#!/bin/bash

echo "Testing circleci build and test locally"
echo

if [ "$#" == "0" ] ; then
    echo "Error: supply job name(s) as arguments, please."
    echo "Example: $0 build test"
    echo " or "
    echo "just $0 test"
    echo "Depending on what jobs you have defined in .circleci/config.yml"
    echo
    echo "Instructions:"
    echo "This scripts simulates running CircleCI automation in the could with"
    echo "/dist, ccache and yum cache preserved between jobs."
    echo "-> run build and tests as they would in the cloud but with local caching"
    
    exit 1
fi

cd `dirname $0`
cd ..

# Test if proxy is needed
wget -q -t 1 --spider www.google.com
if [ "$?" != "0" ] ; then
    export http_proxy=http://wwwproxy.fmi.fi:8080 && \
    export https_proxy=http://wwwproxy.fmi.fi:8080
fi

# Pass some things to circleci environment
ENVSTR="-e LOCALUID=`id -u`"
test -z "$http_proxy" || ENVSTR="$ENVSTR -e http_proxy=$http_proxy"
test -z "$https_proxy" || ENVSTR="$ENVSTR -e https_proxy=$https_proxy"

set -ex
HOST_CCACHE=/tmp/`id -u`-ccache
HOST_YCACHE=/tmp/`id -u`-yum-cache
circleci update install
circleci update build-agent
circleci config validate
docker pull centos:latest
mkdir -p "$HOST_CCACHE"
mkdir -p "$HOST_YCACHE"
mkdir -p dist

while (( "$#" )) ; do
    circleci local execute $ENVSTR -v $HOST_YCACHE:/var/cache/yum -v $HOST_CCACHE:/ccache -v ${PWD}/dist:/root/dist -v ${PWD}/dist:/dist --job "$1"
    shift
done

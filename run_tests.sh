cd `dirname ${BASH_SOURCE[0]}`
[ ! -d "./build" ] && echo "Error: build project first using ./build_debug.sh or ./build_release.sh" && exit
cd build
ctest $*

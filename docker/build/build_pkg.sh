#!/bin/bash

set -e

SCRIPTDIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
source $SCRIPTDIR/calaos_lib.sh

pkgname=$1
repo=$2
arch=$3
commit=$4
pkgversion=$(echo $5 | tr - _)

fix_docker_perms

if [ -z "$pkgname" ]
then
    echo "No package name given. Usage: $0 calaos-ddns|calaos-home|calaos-server <repo> <arch> <commit> <pkgversion>"
    exit 1
fi

if [ -z "$arch" ]
then
    arch="x86_64"
fi

echo "Building package for repo: $repo and arch: $arch"
echo "Commit: $commit"
echo "Package version: $pkgversion"

setup_calaos_repo
import_gpg_key

cd $build_dir/pkgbuilds/$pkgname

#Only change PKGBUILD if it is a calaos package
if grep 'pkgname=.*calaos.*' PKGBUILD > /dev/null
then
    echo "--> Updating PKGBUILD with version/commit"

    #Update PKGBUILD with version
    if [ ! -z "$pkgversion" ]
    then
        sed -E -i "s/pkgver=[0-9\.]+/pkgver=$pkgversion/" PKGBUILD
    else
        echo "--> No version set, use git to get version info"
        #no version set, get version from git
        cat >> PKGBUILD <<- 'EOF'

pkgver() {
  cd "$srcdir/$_pkgdir"
  echo "$(git describe --long --tags --always)" | tr - _
}
EOF
    fi

    #Update PKGBUILD with commit
    if [ ! -z "$commit" ]
    then
        sed -E -i "s/commit=[a-z0-9]+/commit=$commit/" PKGBUILD
    else
        echo "--> No commit, use default branch"
        #no commit set, user default master branch
        sed -E -i "s/#commit=[a-z0-9]+//" PKGBUILD
    fi

    cat PKGBUILD
fi

if [ $signing_available -eq 1 ]
then
    makepkg -f -s --sign --noconfirm
else
    makepkg -f -s --noconfirm
fi

mkdir -p $build_dir/out/pkgs/$arch
cp $build_dir/pkgbuilds/$pkgname/*pkg.tar.zst* $build_dir/out/pkgs/$arch

#Only upload package if repo is set
if [ ! -z "$repo" ]
then
    echo "--> Uploading package to repo $repo"
    upload_pkg $build_dir/pkgbuilds/$pkgname/*pkg.tar.zst $repo $arch
else
    echo "--> Not uploading package"
fi
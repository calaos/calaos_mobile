#!/bin/bash

set -e

SCRIPTDIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
source $SCRIPTDIR/calaos_lib.sh

fix_docker_perms

sync_repo $build_dir/pkgbuilds https://github.com/calaos/pkgbuilds.git master
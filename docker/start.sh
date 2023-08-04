#!/bin/sh

cleanup() {
    echo "Stopping calaos-home and Xorg"

    killall calaos_home
}
trap 'cleanup' TERM EXIT

/usr/bin/startx -- vt8 -keeptty -verbose 3 -logfile /dev/null &
pid=$!

wait $pid
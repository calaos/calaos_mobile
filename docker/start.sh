#!/bin/sh

cleanup() {
    echo "Stopping calaos-home and Xorg"

    killall calaos_home
}
trap 'cleanup' TERM EXIT

# check if arguments --modesetting is set
if [ "$1" = "--modesetting" ]; then
    echo "[*] Using modesetting driver"
    
    #remove intel driver to enable modsetting
    rm -f /usr/lib/xorg/modules/drivers/intel_drv.so
fi

/usr/bin/startx -- vt8 -keeptty -verbose 3 -logfile /dev/null &
pid=$!

wait $pid

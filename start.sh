if ! sudo modprobe tls; then
    echo "kLTS failed to load"
fi

if [ -f "mserv" ]; then
    ./mserv $@
else
    make
    ./mserv $@
fi
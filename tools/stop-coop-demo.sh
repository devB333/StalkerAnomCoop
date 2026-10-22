#!/bin/bash
# Stop a running co-op session gracefully: writes the stop files of the newest demo session (or
# the session given as task/run, e.g. S06-T07/demo-3); the server and both clients exit by
# themselves within a few seconds.
#   bash tools/stop-coop-demo.sh [task/run]
cd "$(dirname "$0")/.." || exit 1
if [ -n "$1" ]; then
    dir="builds/stage5/$1"
else
    dir=$(ls -dt builds/stage5/*/demo-* 2>/dev/null | head -1)
fi
if [ -z "$dir" ] || [ ! -d "$dir" ]; then echo "No such session: ${1:-any demo}"; exit 1; fi
for name in server c1 c2; do touch "$dir/stop-$name"; done
echo "Stop requested for $dir; waiting for the processes to exit..."
for _ in $(seq 1 60); do
    if ! tasklist.exe | grep -qi "Anomaly"; then echo "Stopped."; exit 0; fi
    sleep 1
done
echo "Still running after 60 s; close the game windows by hand."
exit 1

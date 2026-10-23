#!/bin/bash
# A hands-on co-op session on the current build: one campaign server and two client windows on
# this PC, Cordon fixture, both players armed, no test program. Picks the next free run name
# (demo-N) so it can be started again and again.
#   bash tools/start-coop-demo.sh [task] [boar seconds, 0 = no boar] [minutes] [checkpoint seconds, 0 = off]
# Defaults: task S07-T08, a boar 30 s after both players move, 28 minutes, a checkpoint every 120 s
# (S07-T08 play test: watch for a stutter of the other player and NPCs when the server logs
# checkpoint-taken; joins and the clean stop always checkpoint).
set -e
cd "$(dirname "$0")/.."
task=${1:-S07-T08}
boar=${2:-30}
minutes=${3:-28}
checkpoint=${4:-120}
if tasklist.exe | grep -qi "Anomaly"; then
    echo "A server or client is still running; close it (or wait for its session to end) first."
    exit 1
fi
n=1
while [ -e "builds/logs/$task/run-demo-$n" ] || [ -e "builds/stage5/$task/demo-$n" ] \
      || [ -e "builds/stage2/test-install/$task/demo-$n-server" ]; do n=$((n + 1)); done
run="demo-$n"
python3 tools/prepare-coop-session.py "$run" --task "$task" --checkpoint-seconds "$checkpoint" > /dev/null
profile="builds/stage2/test-install/$task/$run-server/coop.ltx"
sed -i "s/^arm = 0/arm = 1/; s/^encounter = 0/encounter = $boar/" "$profile"
secs=$((minutes * 60))
echo "Starting $run ($task): close both game windows when you are done; the server then stops by itself."
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass \
    -File 'C:\Users\djb20\StalkerAnomCoop\tools\Run-CoopSession.ps1' -Run "$run" -Task "$task" \
    -ServerAloneSeconds 10 -ClientStarts '0,10' -ClientSeconds "$secs,$((secs - 10))" -AfterSeconds 10 > /dev/null
echo "$run finished; logs in builds/logs/$task/run-$run"

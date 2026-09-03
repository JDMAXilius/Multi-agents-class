#!/bin/zsh
# AIB22 W-VERIFY: 5 headless matches per map (the verifier's baseline command), two maps in
# parallel, then the parser vs the v2 baselines. Editor must be CLOSED for the build, not for this.
set -u
REPO=${0:A:h:h:h}
ENG="/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd"
TAG=${1:-$(date +%Y%m%d-%H%M)}
run_map() {
  local map=$1 name=$2
  for i in 1 2 3 4 5; do
    "$ENG" "$REPO/Breachpoint.uproject" "/Game/Maps/$map?TargetPlayers=8?Teams=1?TimeLimit=300?ScoreLimit=200" \
      -game -windowed -ResX=640 -ResY=480 -nullrhi -unattended -nopause -nosplash -log -SECONDS=330 \
      -BENCHMARK -FPS=60 "-ini:Game:[/Script/BreachpointNext.BNGameMode]:MinPlayers=0" \
      -LogCmds="LogAIBot Verbose" -abslog="$REPO/Tools/Logs/aib22-verify-$name-$TAG-$i.log" >/dev/null 2>&1
    echo "$name $i done $(date +%H:%M:%S)"
  done
}
run_map BR_Spillway spillway &
run_map BR_Arena01 arena01 &
wait
echo "=== spillway ==="
python3 "$REPO/Tools/aib/80_aib_metrics.py" "$REPO"/Tools/Logs/aib22-verify-spillway-$TAG-*.log --baseline "$REPO/Tools/aib/baselines/aib22-spillway-2026-09-02-v2.json" --json > "$REPO/Tools/aib/baselines/aib22-spillway-verify-$TAG.json"
echo "=== arena01 ==="
python3 "$REPO/Tools/aib/80_aib_metrics.py" "$REPO"/Tools/Logs/aib22-verify-arena01-$TAG-*.log --baseline "$REPO/Tools/aib/baselines/aib22-arena01-2026-09-02-v2.json" --json > "$REPO/Tools/aib/baselines/aib22-arena01-verify-$TAG.json"

#!/bin/bash
# Several trainings side by side in ONE heavy-slot job (threads split over the slot's JOBS), repeated in 20-minute chunks
# (each resumes from RUN_DIR/ckpt.pt) until every RUN_DIR/done exists.
#   nohup bash train/drive_train.sh STEPS "name1:flags..." "name2:flags..." > worktrees/train-scratch/logs/drive_train.log 2>&1 &
set -u
cd "$(dirname "$0")/.."
S=worktrees/train-scratch
steps=$1; shift
SLOT=$HOME/optimist/farm-slot.sh
cat > $S/chunk.sh <<'EOF'
#!/bin/bash
# $1 = steps, rest = "name:flags"
ulimit -n 65536 2>/dev/null || ulimit -n $(ulimit -Hn)
cd "$(dirname "$0")/../.."
S=worktrees/train-scratch
steps=$1; shift
n=$#
th=$(( ${JOBS:-12} / n )); [ $th -lt 1 ] && th=1
for cfg in "$@"; do
    name=${cfg%%:*}; flags=${cfg#*:}
    run=$S/runs/$name
    mkdir -p "$run"
    [ -f "$run/done" ] && continue
    nice -n 19 env OMP_NUM_THREADS=$th .venv/bin/python -I train/train.py $S/feat_traktor $S/labels.json "$run" --steps "$steps" --threads $th $flags > "$run/stdout.txt" 2>&1 &
done
wait
EOF
for try in $(seq 1 40); do
    left=0
    for cfg in "$@"; do [ -f "$S/runs/${cfg%%:*}/done" ] || left=1; done
    [ $left = 0 ] && break
    echo "$(date +%T) chunk $try"
    "$SLOT" heavy bash $S/chunk.sh "$steps" "$@"
done
echo "$(date +%T) finished"

#!/bin/bash

HOST="127.0.0.1"
PORT=8080
SERVER="./cmake-build-debug/MiniDB"

FIRST_BATCH=500
SECOND_BATCH=500

echo "======================================"
echo "MiniDB Compaction Crash Recovery Test"
echo "First batch:   $FIRST_BATCH"
echo "Second batch:  $SECOND_BATCH"
echo "Total keys:    $((FIRST_BATCH + SECOND_BATCH))"
echo "======================================"

wait_for_server()
{
    for i in $(seq 1 50); do

        if nc -z "$HOST" "$PORT" >/dev/null 2>&1; then
            return 0
        fi

        sleep 0.1
    done

    return 1
}


stop_server()
{
    pkill -INT -x MiniDB 2>/dev/null || true

    sleep 1

    pkill -KILL -x MiniDB 2>/dev/null || true
}


echo
echo "Cleaning up old MiniDB process..."

stop_server


echo
echo "Removing old persistence files..."

rm -f minidb.snapshot
rm -f minidb.wal


echo
echo "Starting MiniDB..."

"$SERVER" > compaction_crash_recovery_server.log 2>&1 &
SERVER_PID=$!

if ! wait_for_server; then

    echo "ERROR: MiniDB failed to start."

    cat compaction_crash_recovery_server.log

    exit 1
fi

echo "Server started successfully."


# ============================================================
# FIRST BATCH
# ============================================================

echo
echo "Writing first batch..."

FIRST_COMMANDS=$(mktemp)
FIRST_OUTPUT=$(mktemp)

{
    for i in $(seq 1 "$FIRST_BATCH"); do
        printf 'SET compact_before_%d value_before_%d\n' "$i" "$i"
    done

    printf 'EXIT\n'

} > "$FIRST_COMMANDS"


nc "$HOST" "$PORT" \
    < "$FIRST_COMMANDS" \
    > "$FIRST_OUTPUT" \
    2>/dev/null


FIRST_ERRORS=$(grep -vc '^OK$' "$FIRST_OUTPUT" || true)

# EXIT produces BYE, so don't count it as an error.
FIRST_ERRORS=$((FIRST_ERRORS - 1))

if [[ "$FIRST_ERRORS" -lt 0 ]]; then
    FIRST_ERRORS=0
fi


echo "First batch errors: $FIRST_ERRORS"


rm -f "$FIRST_COMMANDS"
rm -f "$FIRST_OUTPUT"


if [[ "$FIRST_ERRORS" -ne 0 ]]; then

    echo "ERROR: First batch failed."

    stop_server

    exit 1
fi


# ============================================================
# SNAPSHOT
# ============================================================

echo
echo "Creating snapshot..."


SAVE_OUTPUT=$(
    printf 'SAVE\nEXIT\n' |
    nc "$HOST" "$PORT" 2>/dev/null
)


SAVE_RESPONSE=$(printf '%s\n' "$SAVE_OUTPUT" | head -n 1)


echo "SAVE response: $SAVE_RESPONSE"


if [[ "$SAVE_RESPONSE" != "OK" ]]; then

    echo "ERROR: SAVE failed."

    echo
    echo "Server log:"
    cat compaction_crash_recovery_server.log

    stop_server

    exit 1
fi


echo "Snapshot created successfully."


# ============================================================
# SECOND BATCH
#
# These keys are deliberately written AFTER the snapshot.
# They should therefore be recovered from the WAL.
# ============================================================

echo
echo "Writing second batch after snapshot..."


SECOND_COMMANDS=$(mktemp)
SECOND_OUTPUT=$(mktemp)


{
    for i in $(seq 1 "$SECOND_BATCH"); do
        printf 'SET compact_after_%d value_after_%d\n' "$i" "$i"
    done

    printf 'EXIT\n'

} > "$SECOND_COMMANDS"


nc "$HOST" "$PORT" \
    < "$SECOND_COMMANDS" \
    > "$SECOND_OUTPUT" \
    2>/dev/null


SECOND_ERRORS=$(grep -vc '^OK$' "$SECOND_OUTPUT" || true)

# EXIT produces BYE, so don't count it as an error.
SECOND_ERRORS=$((SECOND_ERRORS - 1))

if [[ "$SECOND_ERRORS" -lt 0 ]]; then
    SECOND_ERRORS=0
fi


echo "Second batch errors: $SECOND_ERRORS"


rm -f "$SECOND_COMMANDS"
rm -f "$SECOND_OUTPUT"


if [[ "$SECOND_ERRORS" -ne 0 ]]; then

    echo "ERROR: Second batch failed."

    stop_server

    exit 1
fi


# ============================================================
# ABRUPT CRASH
# ============================================================

echo
echo "Simulating abrupt crash..."


kill -KILL "$SERVER_PID"

wait "$SERVER_PID" 2>/dev/null || true


sleep 1


if nc -z "$HOST" "$PORT" >/dev/null 2>&1; then

    echo "ERROR: MiniDB is still running after crash."

    exit 1
fi


echo "MiniDB terminated abruptly."


# ============================================================
# RESTART
# ============================================================

echo
echo "Restarting MiniDB..."


"$SERVER" > compaction_crash_recovery_restart.log 2>&1 &
RESTART_PID=$!


if ! wait_for_server; then

    echo "ERROR: MiniDB failed to restart."

    echo
    echo "Restart log:"
    cat compaction_crash_recovery_restart.log

    exit 1
fi


echo "Server restarted successfully."


# ============================================================
# VERIFY FIRST BATCH
#
# These keys should come from the snapshot.
# ============================================================

echo
echo "Verifying first batch..."


FIRST_MISSING=0


for i in $(seq 1 "$FIRST_BATCH"); do

    response=$(
        printf 'GET compact_before_%d\nEXIT\n' "$i" |
        nc "$HOST" "$PORT" 2>/dev/null |
        head -n 1
    )


    if [[ "$response" != "value_before_$i" ]]; then
        FIRST_MISSING=$((FIRST_MISSING + 1))
    fi

done


echo "First batch missing/incorrect: $FIRST_MISSING"


# ============================================================
# VERIFY SECOND BATCH
#
# These keys should come from WAL replay.
# ============================================================

echo
echo "Verifying second batch..."


SECOND_MISSING=0


for i in $(seq 1 "$SECOND_BATCH"); do

    response=$(
        printf 'GET compact_after_%d\nEXIT\n' "$i" |
        nc "$HOST" "$PORT" 2>/dev/null |
        head -n 1
    )


    if [[ "$response" != "value_after_$i" ]]; then
        SECOND_MISSING=$((SECOND_MISSING + 1))
    fi

done


echo "Second batch missing/incorrect: $SECOND_MISSING"


# ============================================================
# RESULT
# ============================================================

TOTAL_ERRORS=$((FIRST_MISSING + SECOND_MISSING))


echo
echo "======================================"


if [[ "$TOTAL_ERRORS" -eq 0 ]]; then

    echo "Compaction crash recovery PASSED"

else

    echo "Compaction crash recovery FAILED"
    echo "Total missing/incorrect: $TOTAL_ERRORS"

fi


echo "======================================"


# ============================================================
# CLEAN SHUTDOWN AFTER TEST
# ============================================================

kill -INT "$RESTART_PID" 2>/dev/null || true

wait "$RESTART_PID" 2>/dev/null || true


exit "$TOTAL_ERRORS"
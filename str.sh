#!/bin/bash

HOST="127.0.0.1"
PORT=8080

echo "======================================"
echo "MiniDB Concurrent SAVE Test"
echo "======================================"

# Verify server
if ! printf "PING\n" | nc -q 0 -w 2 "$HOST" "$PORT" | grep -q "^PONG$"; then
    echo "ERROR: MiniDB is not running."
    exit 1
fi

echo "Server is available."

# Writer: continuously create/update keys
writer() {
    for i in $(seq 1 500); do
        printf "SET save_key_%d save_value_%d\n" "$i" "$i" |
            nc -q 0 -w 2 "$HOST" "$PORT" >/dev/null
    done
}

# Saver: repeatedly compact while writes are happening
saver() {
    for i in $(seq 1 10); do
        RESULT=$(printf "SAVE\n" |
            nc -q 0 -w 5 "$HOST" "$PORT")

        if [ "$RESULT" != "OK" ]; then
            echo "SAVE $i FAILED: '$RESULT'"
            return 1
        fi

        echo "SAVE $i OK"
    done
}

echo
echo "Starting concurrent writers and SAVE operations..."

writer &
WRITER_PID=$!

saver &
SAVER_PID=$!

wait "$WRITER_PID"
WRITER_RESULT=$?

wait "$SAVER_PID"
SAVER_RESULT=$?

echo
echo "Writer result: $WRITER_RESULT"
echo "Saver result: $SAVER_RESULT"

if [ "$WRITER_RESULT" -ne 0 ] || [ "$SAVER_RESULT" -ne 0 ]; then
    echo "Concurrent SAVE test FAILED."
    exit 1
fi

echo
echo "Checking final data..."

ERRORS=0

for i in 1 500; do
    RESULT=$(printf "GET save_key_%d\n" "$i" |
        nc -q 0 -w 2 "$HOST" "$PORT")

    if [ "$RESULT" != "save_value_$i" ]; then
        echo "ERROR: save_key_$i -> '$RESULT'"
        ERRORS=$((ERRORS + 1))
    fi
done

echo
echo "Verification errors: $ERRORS"

if [ "$ERRORS" -ne 0 ]; then
    echo "Concurrent SAVE test FAILED."
    exit 1
fi

echo
echo "======================================"
echo "Concurrent SAVE test PASSED"
echo "======================================"
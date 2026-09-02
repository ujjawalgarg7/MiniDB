#!/bin/bash

HOST="127.0.0.1"
PORT=8080
SERVER="./cmake-build-debug/MiniDB"

KEY_COUNT=100

echo "======================================"
echo "MiniDB Multi-Key Crash Recovery Test"
echo "Keys: $KEY_COUNT"
echo "======================================"

# --------------------------------------------------
# Make sure server is not already running
# --------------------------------------------------

if printf "PING\n" | nc -w 1 "$HOST" "$PORT" 2>/dev/null | grep -q "^PONG$"; then
    echo "ERROR: MiniDB is already running on port $PORT."
    echo "Stop it before running this test."
    exit 1
fi

# --------------------------------------------------
# Start MiniDB
# --------------------------------------------------

echo
echo "Starting MiniDB..."

"$SERVER" > crash_recovery_server.log 2>&1 &
SERVER_PID=$!

echo "MiniDB PID: $SERVER_PID"

READY=0

for ((i=1; i<=50; i++)); do
    if printf "PING\n" | nc -w 1 "$HOST" "$PORT" 2>/dev/null | grep -q "^PONG$"; then
        READY=1
        break
    fi

    sleep 0.1
done

if [ "$READY" -ne 1 ]; then
    echo "ERROR: MiniDB failed to start."
    cat crash_recovery_server.log

    kill "$SERVER_PID" 2>/dev/null
    wait "$SERVER_PID" 2>/dev/null

    exit 1
fi

echo "Server is ready."

# --------------------------------------------------
# Write 100 keys
# --------------------------------------------------

echo
echo "Writing $KEY_COUNT keys..."

SUCCESSFUL_WRITES=0

for ((i=1; i<=KEY_COUNT; i++)); do
    KEY="crash_key_$i"
    VALUE="crash_value_$i"

    RESULT=$(printf "SET %s %s\n" "$KEY" "$VALUE" |
        nc -q 0 -w 2 "$HOST" "$PORT")

    if [ "$RESULT" = "OK" ]; then
        SUCCESSFUL_WRITES=$((SUCCESSFUL_WRITES + 1))
    else
        echo "ERROR: SET failed for $KEY"
    fi
done

echo "Successful writes: $SUCCESSFUL_WRITES/$KEY_COUNT"

if [ "$SUCCESSFUL_WRITES" -ne "$KEY_COUNT" ]; then
    echo "ERROR: Not all keys were written."

    kill "$SERVER_PID" 2>/dev/null
    wait "$SERVER_PID" 2>/dev/null

    exit 1
fi

# --------------------------------------------------
# Crash
# --------------------------------------------------

echo
echo "Simulating crash..."

kill -9 "$SERVER_PID"
wait "$SERVER_PID" 2>/dev/null

echo "MiniDB terminated abruptly."

# --------------------------------------------------
# Restart
# --------------------------------------------------

echo
echo "Restarting MiniDB..."

"$SERVER" > crash_recovery_server_restart.log 2>&1 &
SERVER_PID=$!

READY=0

for ((i=1; i<=50; i++)); do
    if printf "PING\n" | nc -w 1 "$HOST" "$PORT" 2>/dev/null | grep -q "^PONG$"; then
        READY=1
        break
    fi

    sleep 0.1
done

if [ "$READY" -ne 1 ]; then
    echo "ERROR: MiniDB failed to restart."
    cat crash_recovery_server_restart.log

    kill "$SERVER_PID" 2>/dev/null
    wait "$SERVER_PID" 2>/dev/null

    exit 1
fi

echo "Server restarted successfully."

# --------------------------------------------------
# Verify all keys
# --------------------------------------------------

echo
echo "Checking recovered data..."

RECOVERED=0
MISSING=0

for ((i=1; i<=KEY_COUNT; i++)); do
    KEY="crash_key_$i"
    EXPECTED="crash_value_$i"

    RESULT=$(printf "GET %s\n" "$KEY" |
        nc -q 0 -w 2 "$HOST" "$PORT")

    if [ "$RESULT" = "$EXPECTED" ]; then
        RECOVERED=$((RECOVERED + 1))
    else
        MISSING=$((MISSING + 1))
        echo "ERROR: $KEY -> expected '$EXPECTED', got '$RESULT'"
    fi
done

echo
echo "Recovered: $RECOVERED/$KEY_COUNT"
echo "Missing/incorrect: $MISSING"

# --------------------------------------------------
# Stop restarted server
# --------------------------------------------------

kill "$SERVER_PID" 2>/dev/null
wait "$SERVER_PID" 2>/dev/null

# --------------------------------------------------
# Final result
# --------------------------------------------------

if [ "$RECOVERED" -ne "$KEY_COUNT" ]; then
    echo
    echo "Crash recovery test FAILED."
    exit 1
fi

echo
echo "======================================"
echo "Crash recovery test PASSED"
echo "======================================"

exit 0
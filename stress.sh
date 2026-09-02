#!/bin/bash

HOST="127.0.0.1"
PORT=8080

CLIENTS=20
OPS_PER_CLIENT=100
TOTAL_OPS=$((CLIENTS * OPS_PER_CLIENT))

echo "======================================"
echo "MiniDB Concurrent Stress Test"
echo "Clients:    $CLIENTS"
echo "Operations: $OPS_PER_CLIENT per client"
echo "Total:      $TOTAL_OPS"
echo "======================================"

echo
echo "Checking MiniDB server..."

if ! printf "PING\n" | nc -w 2 "$HOST" "$PORT" | grep -q "^PONG$"; then
    echo "ERROR: MiniDB server is not running on $HOST:$PORT"
    exit 1
fi

echo "Server is available."

rm -f stress_client_*.out

START_TIME=$(date +%s)

echo
echo "Starting $CLIENTS concurrent clients..."

for ((client=1; client<=CLIENTS; client++)); do
    (
        output_file="stress_client_${client}.out"

        for ((op=1; op<=OPS_PER_CLIENT; op++)); do
            key="stress_${client}_${op}"
            value="value_${client}_${op}"

            if ! timeout 5s bash -c \
                "printf 'SET %s %s\n' '$key' '$value' | nc -q 0 -w 2 '$HOST' '$PORT'" \
                >> "$output_file" 2>&1
            then
                echo "ERROR client=$client operation=$op" >> "$output_file"
            fi
        done

    ) &
done

# Give the entire stress test a maximum runtime.
TEST_TIMEOUT=30

SECONDS_WAITED=0

while jobs -rp | grep -q .; do
    sleep 1
    SECONDS_WAITED=$((SECONDS_WAITED + 1))

    if [ "$SECONDS_WAITED" -ge "$TEST_TIMEOUT" ]; then
        echo
        echo "ERROR: Stress test exceeded ${TEST_TIMEOUT} seconds."
        echo "Stopping client processes..."

        pkill -P $$ 2>/dev/null
        exit 1
    fi
done

END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))

echo
echo "Stress test finished."
echo "Time: ${ELAPSED} seconds"

echo
echo "Checking client results..."

ERRORS=$(grep -h -E "ERROR|ERR|Connection refused|failed" \
    stress_client_*.out 2>/dev/null | wc -l)

echo "Errors: $ERRORS"

echo
echo "Counting successful operations..."

SUCCESSFUL_SETS=$(grep -h -c "^OK$" stress_client_*.out 2>/dev/null |
    awk '{sum += $1} END {print sum+0}')

echo "Successful SETs: $SUCCESSFUL_SETS"

if [ "$ERRORS" -ne 0 ]; then
    echo
    echo "ERROR: Client errors detected."
    echo "Stress test FAILED."
    exit 1
fi

if [ "$SUCCESSFUL_SETS" -ne "$TOTAL_OPS" ]; then
    echo
    echo "ERROR: Expected $TOTAL_OPS successful SETs."
    echo "ERROR: Received $SUCCESSFUL_SETS successful SETs."
    echo "Stress test FAILED."
    exit 1
fi

echo
echo "Checking server INFO..."

INFO_OUTPUT=$(printf "INFO\n" |
    nc -q 0 -w 5 "$HOST" "$PORT" 2>/dev/null)

if [ -z "$INFO_OUTPUT" ]; then
    echo "ERROR: Server did not return INFO."
    echo "Stress test FAILED."
    exit 1
fi

echo "$INFO_OUTPUT"

echo
echo "======================================"
echo "Stress test complete"
echo "======================================"

echo "Stress test PASSED."

exit 0
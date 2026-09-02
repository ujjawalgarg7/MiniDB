#!/bin/bash

HOST="127.0.0.1"
PORT="8080"

CLIENTS=20
OPERATIONS=100

echo "======================================"
echo "MiniDB Concurrent Stress Test"
echo "Clients:    $CLIENTS"
echo "Operations: $OPERATIONS per client"
echo "Total:      $((CLIENTS * OPERATIONS))"
echo "======================================"

run_client() {

    local client_id="$1"

    {
        for ((i=1; i<=OPERATIONS; i++)); do

            key="client${client_id}_key${i}"
            value="value_${client_id}_${i}"

            echo "SET $key $value"

        done

        echo "INFO"

    } | nc -q 0 "$HOST" "$PORT" \
        > "stress_client_${client_id}.out"
}


START=$(date +%s)


for ((client=1; client<=CLIENTS; client++)); do
    run_client "$client" &
done


wait


END=$(date +%s)


echo
echo "Stress test finished."
echo "Time: $((END - START)) seconds"


echo
echo "Checking client results..."


ERRORS=$(
    grep -h "^ERR" stress_client_*.out 2>/dev/null |
    wc -l
)


echo "Errors: $ERRORS"


echo
echo "Counting successful operations..."


SUCCESSFUL=$(
    grep -h "^OK$" stress_client_*.out 2>/dev/null |
    wc -l
)


echo "Successful SETs: $SUCCESSFUL"


echo
echo "Checking server INFO..."


echo "INFO" |
    nc -q 0 "$HOST" "$PORT"


echo
echo "======================================"
echo "Stress test complete"
echo "======================================"
if [ "$ERRORS" -ne 0 ]; then
    echo "Stress test FAILED."
    exit 1
fi

echo "Stress test PASSED."
exit 0

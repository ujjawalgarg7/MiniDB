#!/bin/bash

HOST="127.0.0.1"
PORT=8080

CLIENTS=20
OPERATIONS=100

echo "======================================"
echo "MiniDB Concurrent SET/GET/DEL Test"
echo "Clients:    $CLIENTS"
echo "Operations: $OPERATIONS per client"
echo "Total:      $((CLIENTS * OPERATIONS))"
echo "======================================"

if ! printf "PING\n" | nc -q 0 -w 2 "$HOST" "$PORT" | grep -q "^PONG$"; then
    echo "ERROR: MiniDB is not running."
    exit 1
fi

echo "Server is available."

rm -f concurrent_ops_*.out

worker() {
    CLIENT_ID="$1"
    OUTPUT="concurrent_ops_${CLIENT_ID}.out"

    errors=0

    for i in $(seq 1 "$OPERATIONS"); do

        KEY="concurrent_${CLIENT_ID}_${i}"
        VALUE="value_${CLIENT_ID}_${i}"

        # SET
        RESULT=$(printf "SET %s %s\n" "$KEY" "$VALUE" |
            nc -q 0 -w 2 "$HOST" "$PORT")

        if [ "$RESULT" != "OK" ]; then
            echo "SET ERROR: $KEY -> '$RESULT'" >> "$OUTPUT"
            errors=$((errors + 1))
            continue
        fi

        # GET
        RESULT=$(printf "GET %s\n" "$KEY" |
            nc -q 0 -w 2 "$HOST" "$PORT")

        if [ "$RESULT" != "$VALUE" ]; then
            echo "GET ERROR: $KEY -> expected '$VALUE', got '$RESULT'" >> "$OUTPUT"
            errors=$((errors + 1))
        fi

        # DEL every 5th key
        if (( i % 5 == 0 )); then

            RESULT=$(printf "DEL %s\n" "$KEY" |
                nc -q 0 -w 2 "$HOST" "$PORT")

            if [ "$RESULT" != "1" ]; then
                echo "DEL ERROR: $KEY -> '$RESULT'" >> "$OUTPUT"
                errors=$((errors + 1))
            fi

            # Confirm deletion
            RESULT=$(printf "GET %s\n" "$KEY" |
                nc -q 0 -w 2 "$HOST" "$PORT")

            if [ "$RESULT" != "(nil)" ]; then
                echo "DELETE VERIFY ERROR: $KEY -> '$RESULT'" >> "$OUTPUT"
                errors=$((errors + 1))
            fi
        fi

    done

    echo "$errors" > "${OUTPUT}.count"
}

echo
echo "Starting $CLIENTS concurrent clients..."

for client in $(seq 1 "$CLIENTS"); do
    worker "$client" &
done

wait

echo
echo "All clients finished."

TOTAL_ERRORS=0

for client in $(seq 1 "$CLIENTS"); do
    COUNT=$(cat "concurrent_ops_${client}.out.count")

    if [ "$COUNT" -ne 0 ]; then
        echo "Client $client errors: $COUNT"
    fi

    TOTAL_ERRORS=$((TOTAL_ERRORS + COUNT))
done

echo
echo "Total errors: $TOTAL_ERRORS"

echo
echo "Checking surviving keys..."

VERIFY_ERRORS=0

for client in $(seq 1 "$CLIENTS"); do
    for i in $(seq 1 "$OPERATIONS"); do

        KEY="concurrent_${client}_${i}"

        # Every 5th key was deliberately deleted.
        if (( i % 5 == 0 )); then
            EXPECTED="(nil)"
        else
            EXPECTED="value_${client}_${i}"
        fi

        RESULT=$(printf "GET %s\n" "$KEY" |
            nc -q 0 -w 2 "$HOST" "$PORT")

        if [ "$RESULT" != "$EXPECTED" ]; then
            echo "VERIFY ERROR: $KEY -> expected '$EXPECTED', got '$RESULT'"
            VERIFY_ERRORS=$((VERIFY_ERRORS + 1))
        fi
    done
done

echo
echo "Verification errors: $VERIFY_ERRORS"

rm -f concurrent_ops_*.out concurrent_ops_*.out.count

if [ "$TOTAL_ERRORS" -ne 0 ] || [ "$VERIFY_ERRORS" -ne 0 ]; then
    echo
    echo "======================================"
    echo "Concurrent SET/GET/DEL test FAILED"
    echo "======================================"
    exit 1
fi

echo
echo "======================================"
echo "Concurrent SET/GET/DEL test PASSED"
echo "======================================"

exit 0

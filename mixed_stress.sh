#!/bin/bash

HOST="127.0.0.1"
PORT="8080"

CLIENTS=20
OPERATIONS=100

echo "======================================"
echo "MiniDB Mixed Concurrent Stress Test"
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

            case $((i % 6)) in

                0)
                    echo "SET $key $value"
                    ;;

                1)
                    echo "GET $key"
                    ;;

                2)
                    echo "SET $key $value EX 30"
                    ;;

                3)
                    echo "EXISTS $key"
                    ;;

                4)
                    echo "DEL $key"
                    ;;

                5)
                    echo "SET $key $value"
                    ;;

            esac

        done

        echo "INFO"

    } | nc -q 0 "$HOST" "$PORT" \
        > "mixed_client_${client_id}.out"
}


START=$(date +%s)


for ((client=1; client<=CLIENTS; client++)); do
    run_client "$client" &
done


wait


END=$(date +%s)


echo
echo "Mixed stress test finished."
echo "Time: $((END - START)) seconds"


echo
echo "Checking errors..."


ERRORS=$(
    grep -h "^ERR" mixed_client_*.out 2>/dev/null |
    wc -l
)


echo "Errors: $ERRORS"


echo
echo "Checking server INFO..."


echo "INFO" |
    nc -q 0 "$HOST" "$PORT"


echo
echo "======================================"
echo "Mixed stress test complete"
echo "======================================"
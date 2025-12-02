#!/bin/bash

NUM=1000
success=0
fail=0
pids=()

# Launch curls in parallel
for i in $(seq 1 $NUM); do
    (
        code=$(curl -v "http://[::1]:8080/" \
            -o /dev/null -s -w "%{http_code}" 2>/dev/null)

        if [ "$code" -eq 200 ]; then
            echo "SUCCESS"
        else
            echo "FAIL (HTTP $code)"
        fi
    ) &
    pids+=($!)
done

# Wait for all and count results
for pid in "${pids[@]}"; do
    if wait "$pid"; then
        ((success++))
    else
        ((fail++))
    fi
done

echo "Success: $success"
echo "Fail: $fail"
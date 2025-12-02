#!/bin/bash

success=0
fail=0

for i in {1..10}; do
    if curl -v "http://[::1]:8080/" \
        -o /dev/null -s -w "%{http_code}" 2>/dev/null \
        | grep -q "^200$"; then
        success=$((success+1))
    else
        fail=$((fail+1))
    fi
done

echo "Success: $success"
echo "Fail: $fail"

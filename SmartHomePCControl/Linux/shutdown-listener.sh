#!/bin/bash

PORT="$1"
KEYWORD="shutdown-my-pc"

while true; do
  echo "Listening on port $PORT..."
  # Use nc to listen on $PORT, receive a single line, and store it
  MESSAGE=$(nc -l -p "$PORT" -q 1)

  echo "Received: $MESSAGE"

  if [[ "$MESSAGE" == "$KEYWORD" ]]; then
    echo "Shutdown command received. Shutting down..."
    shutdown now
    exit 0
  fi
done

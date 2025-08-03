#!/bin/bash

PORT="$1"
KEYWORD="shutdown-my-pc"

if [[ -z "$PORT" ]]; then
  echo "Usage: $0 <port>"
  exit 1
fi

while true; do
  echo "Listening on port $PORT..."
  MESSAGE=$(nc -l "$PORT")

  echo "Received: $MESSAGE"

  if [[ "$MESSAGE" == "$KEYWORD" ]]; then
    echo "Shutdown command received. Shutting down..."
    sudo shutdown -h now
    exit 0
  fi
done

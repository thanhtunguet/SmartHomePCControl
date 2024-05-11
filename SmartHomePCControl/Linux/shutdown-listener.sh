#!/bin/bash

# Set the port number
PORT=10675

# Function to handle shutdown
shutdown_computer() {
    echo "Shutting down the computer..."
    sudo shutdown now
}

# Main function
main() {
# Start listening on the specified port
    echo "Listening for shutdown command on port $PORT..."
    while true; do
# Listen for incoming messages on the specified port
        MESSAGE=$(nc -l -p $PORT)
        
        # Check if the received message is the shutdown command
        if [ "$MESSAGE" == "shutdown-my-pc" ]; then
            shutdown_computer
        else
            echo "Received unknown message: $MESSAGE"
        fi
    done
}

# Execute the main function
main

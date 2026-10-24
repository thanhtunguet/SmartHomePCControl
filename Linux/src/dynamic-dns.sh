    #!/bin/bash

# Load environment variables from ~/.ddns or .env if exists
if [ -f "$HOME/.ddns" ]; then
    set -a
    # shellcheck source=/dev/null
    source "$HOME/.ddns"
    set +a
elif [ -f ".ddns" ]; then
    set -a
    # shellcheck source=/dev/null
    source ".ddns"
    set +a
fi

# Cloudflare Configuration
CF_API_KEY="${CF_API_KEY:-CLOUFLARE_API_KEY}"
CF_EMAIL="${CF_EMAIL:-your_email@example.com}"
ZONE_ID="${ZONE_ID:-ZONE_ID}"
RECORD_ID="${RECORD_ID:-RECORD_ID}"

# Telegram Bot Configuration
BOT_TOKEN="${BOT_TOKEN:-Bot_token}"
CHAT_ID="${CHAT_ID:-Your chat id}"

# DNS Configuration
YOUR_DOMAIN="${YOUR_DOMAIN:-*.example.com}"
DNS_SERVER="${DNS_SERVER:-8.8.8.8}"

    # Function to send a message via Telegram
    function sendChat() {
        curl -s -X POST "https://api.telegram.org/bot${BOT_TOKEN}/sendMessage" \
            -d "chat_id=${CHAT_ID}" \
            -d "text=$1"
    }

    # Function to call Cloudflare API
    function cloudflare() {
    if [ -z "$CF_EMAIL" ]; then
        curl -sSL \
        -H "Accept: application/json" \
        -H "Content-Type: application/json" \
        -H "Authorization: Bearer $CF_API_KEY" \
        "$@"
    else
        curl -sSL \
        -H "Accept: application/json" \
        -H "Content-Type: application/json" \
        -H "X-Auth-Email: $CF_EMAIL" \
        -H "X-Auth-Key: $CF_API_KEY" \
        "$@"
    fi
    }

    # Get current public IP address of your network
    PUBLIC_IP=$(curl -s https://checkip.amazonaws.com)

    # Get current DNS setting of your domain
    CURRENT_IP=$(nslookup "${YOUR_DOMAIN}" ${DNS_SERVER} | awk '/^Address: / { print $2 }')

    # Compare the two IP addresses
    if [ "$CURRENT_IP" != "$PUBLIC_IP" ]; then
        # If the IP address has changed, update DNS and send a notification
        sendChat "The public IP address has changed from: ${CURRENT_IP} to ${PUBLIC_IP}. DNS setting has been updated!"
        cloudflare -X PUT \
        "https://api.cloudflare.com/client/v4/zones/${ZONE_ID}/dns_records/${RECORD_ID}" \
        --data "{\"type\":\"A\",\"name\":\"*\",\"content\":\"${PUBLIC_IP}\",\"ttl\":1,\"proxied\":false}"
    else
        # If the DNS setting is up to date, do nothing
        echo "Current DNS setting is up to date"
    fi

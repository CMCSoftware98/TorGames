#!/bin/bash

# TorGames Server Deployment Script
# This script clones/updates the repo, builds the server, and runs it

set -e  # Exit on any error

REPO_URL="https://github.com/CMCSoftware98/TorGames"
REPO_DIR="$HOME/TorGames"
PUBLISH_DIR="$HOME/publish"

echo "=== TorGames Server Deployment ==="

# Stop any running server
echo "Stopping any running TorGames.Server..."
pkill -f TorGames.Server || true

# Clone or update repository
if [ -d "$REPO_DIR" ]; then
    echo "Updating existing repository..."
    cd "$REPO_DIR"
    git fetch origin
    git reset --hard origin/main
else
    echo "Cloning repository..."
    cd "$HOME"
    git clone "$REPO_URL"
    cd "$REPO_DIR"
fi

# Create directories
echo "Creating directories..."
mkdir -p "$PUBLISH_DIR/server"

# Backup existing appsettings.json if it exists
if [ -f "$PUBLISH_DIR/server/appsettings.json" ]; then
    echo "Backing up existing appsettings.json..."
    cp "$PUBLISH_DIR/server/appsettings.json" "$PUBLISH_DIR/server/appsettings.json.bak"
fi

# Build and publish the server
echo "Building TorGames.Server..."
dotnet publish TorGames.Server/TorGames.Server.csproj \
    -c Release \
    -r linux-x64 \
    --self-contained true \
    -o "$PUBLISH_DIR/server"

# Restore appsettings.json (preserve existing settings if they exist)
if [ -f "$PUBLISH_DIR/server/appsettings.json.bak" ]; then
    echo "Restoring previous appsettings.json..."
    cp "$PUBLISH_DIR/server/appsettings.json.bak" "$PUBLISH_DIR/server/appsettings.json"
else
    echo "Copying default appsettings.json..."
    cp TorGames.Server/appsettings.json "$PUBLISH_DIR/server/"
fi

# Set executable permissions
echo "Setting executable permissions..."
chmod +x "$PUBLISH_DIR/server/TorGames.Server"

echo ""
echo "=== Deployment Complete ==="
echo "Server published to: $PUBLISH_DIR/server"
echo ""
echo "To start the server, run:"
echo "  cd $PUBLISH_DIR/server && ./TorGames.Server"
echo ""
echo "Or to start in background:"
echo "  cd $PUBLISH_DIR/server && nohup ./TorGames.Server > server.log 2>&1 &"
echo ""

# Ask if user wants to start the server
read -p "Start the server now? (y/n): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "Starting TorGames.Server..."
    cd "$PUBLISH_DIR/server"
    ./TorGames.Server
fi

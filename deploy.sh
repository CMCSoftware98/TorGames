#!/bin/bash

# TorGames Server Deployment Script
# This script clones/updates the repo, builds the server and updater, and runs the server

set -e  # Exit on any error

REPO_URL="https://github.com/CMCSoftware98/TorGames"
REPO_DIR="$HOME/TorGames"
PUBLISH_DIR="$HOME/publish"
UPDATES_DIR="$HOME/updates"

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
mkdir -p "$UPDATES_DIR/updater"

# Build and publish the server
echo "Building TorGames.Server..."
dotnet publish TorGames.Server/TorGames.Server.csproj \
    -c Release \
    -r linux-x64 \
    --self-contained true \
    -o "$PUBLISH_DIR/server"

# Copy appsettings.json (preserve existing settings if they exist)
if [ -f "$PUBLISH_DIR/server/appsettings.json.bak" ]; then
    echo "Restoring previous appsettings.json..."
    cp "$PUBLISH_DIR/server/appsettings.json.bak" "$PUBLISH_DIR/server/appsettings.json"
else
    echo "Copying default appsettings.json..."
    cp TorGames.Server/appsettings.json "$PUBLISH_DIR/server/"
fi

# Build and publish the updater (Windows x64 - for Windows clients)
echo "Building TorGames.Updater for Windows..."
dotnet publish TorGames.Updater/TorGames.Updater.csproj \
    -c Release \
    -r win-x64 \
    --self-contained true \
    -p:PublishSingleFile=true \
    -o "$UPDATES_DIR/updater"

# Make server executable
chmod +x "$PUBLISH_DIR/server/TorGames.Server"

echo ""
echo "=== Deployment Complete ==="
echo "Server published to: $PUBLISH_DIR/server"
echo "Updater published to: $UPDATES_DIR/updater"
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

#!/bin/bash
# Setup Mendeley dataset environment with Redis AOF
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== Mendeley Dataset Redis AOF Setup ==="
echo ""

# Step 1: Create directories
echo "Creating directories..."
mkdir -p data redis_data logs

# Step 2: Extract dataset if not already extracted
if [ ! -d "data/Real Data Sets" ]; then
    echo "Extracting dataset..."
    if [ ! -f "kxcb3tnr3t-1.zip" ]; then
        echo "ERROR: Dataset zip file not found: kxcb3tnr3t-1.zip"
        echo "Please download from: https://data.mendeley.com/datasets/kxcb3tnr3t/1"
        exit 1
    fi
    unzip -q -o kxcb3tnr3t-1.zip -d data/
    FILE_COUNT=$(find data -type f | wc -l)
    echo "Extracted $FILE_COUNT files"
else
    echo "Dataset already extracted"
fi

# Step 3: Stop any existing Redis Docker container
echo "Stopping any existing Redis Docker container..."
pkill -f "redis-server" 2>/dev/null || true
docker stop redis-aof 2>/dev/null || true
docker rm redis-aof 2>/dev/null || true
sleep 1

# Step 4: Clean up old Redis data
echo "Cleaning up old Redis data..."
rm -rf appendonlydir/ appendonly.aof dump.rdb redis_data/*.pid

# Step 5: Build and start Redis Docker container
echo "Building Docker image..."
docker build -t redis-aof .

echo "Starting Redis in Docker..."
docker run -d \
  --name redis-aof \
  -p 6379:6379 \
  -v $(pwd)/redis_data:/data \
  -v $(pwd)/redis.conf:/usr/local/etc/redis/redis.conf:ro \
  redis-aof \
  redis-server /usr/local/etc/redis/redis.conf

sleep 2

# Wait for Redis to be ready
echo "Waiting for Redis..."
for i in {1..30}; do
    if redis-cli -p 6379 ping > /dev/null 2>&1; then
        echo "Redis is ready!"
        break
    fi
    sleep 1
    if [ $i -eq 30 ]; then
        echo "ERROR: Redis failed to start"
        echo "Check logs at: $SCRIPT_DIR/logs/redis.log"
        exit 1
    fi
done

# Step 6: Load datasets into Redis
echo ""
echo "Loading datasets into Redis..."
# Use venv if available, otherwise use system python3
../venv/bin/python3 load_datasets.py

# Step 7: Show results
echo ""
echo "=== Setup Complete ==="
CONTAINER_STATUS=$(docker ps --filter "name=redis-aof" --format "{{.Status}}" || echo "Not running")
echo "Docker container: $CONTAINER_STATUS"
KEYS_COUNT=$(redis-cli DBSIZE 2>/dev/null || echo "Cannot connect")
echo "Keys in database: $KEYS_COUNT"

echo ""
echo "✓ Setup complete!"
echo ""
echo "To stop Redis: docker stop redis-aof"
echo "To restart: docker start redis-aof"
echo "To rebuild and restart: ./setup.sh"


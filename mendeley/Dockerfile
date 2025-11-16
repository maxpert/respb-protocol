# Dockerfile for Redis 6.2.14 with AOF for RESP analysis
# Uses simple single-file AOF format (pre-Redis 7.0)

FROM redis:6.2.14

# Expose Redis port
EXPOSE 6379

# Set working directory to /data where AOF will be stored
WORKDIR /data

# Use the redis.conf from the mendeley directory (mounted at runtime)
# Run command: docker build -t redis-aof . && docker run -d -p 6379:6379 -v $(pwd):/data -v $(pwd)/redis.conf:/usr/local/etc/redis/redis.conf redis-aof redis-server /usr/local/etc/redis/redis.conf
CMD ["redis-server"]

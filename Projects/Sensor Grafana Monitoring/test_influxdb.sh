#!/bin/bash
# ──────────────────────────────────────────────────────────
#  Test script: sends fake pH/Temperature data to InfluxDB 3
#  then queries it back to verify the pipeline.
#
#  Usage:  bash test_influxdb.sh
# ──────────────────────────────────────────────────────────

INFLUX_URL="http://localhost:8181"
DATABASE="sensor_monitoring"

echo ""
echo "══════════════════════════════════════"
echo "  InfluxDB 3 Pipeline Test"
echo "══════════════════════════════════════"
echo ""

# 1. Show container entrypoints
echo "─── Container Status ───"
docker ps --format "table {{.Names}}\t{{.Status}}\t{{.Ports}}" 2>/dev/null
echo ""

# 2. Create database (safe to run if it already exists)
echo "─── Creating database '$DATABASE' ───"
docker exec influxdb3 influxdb3 create database $DATABASE --host $INFLUX_URL 2>&1
echo ""

# 3. Write 10 fake water quality data points
echo "─── Writing 10 test points ───"
for i in $(seq 1 10); do
  # Generate random pH (5.5–8.5) and temperature (18–32)
  PH=$(awk "BEGIN {printf \"%.2f\", 5.5 + rand() * 3.0}")
  TEMP=$(awk "BEGIN {printf \"%.1f\", 18.0 + rand() * 14.0}")

  RESPONSE=$(curl -s -o /dev/null -w "%{http_code}" -X POST \
    "${INFLUX_URL}/api/v2/write?bucket=${DATABASE}&precision=ms" \
    -H "Content-Type: text/plain" \
    -d "water_quality,sensor=ph_temp ph=${PH},temperature=${TEMP}")

  if [ "$RESPONSE" = "200" ] || [ "$RESPONSE" = "204" ]; then
    echo "  ✓ Point $i: pH=$PH  temp=$TEMP °C  (HTTP $RESPONSE)"
  else
    echo "  ✗ Point $i FAILED (HTTP $RESPONSE)"
  fi
  sleep 1
done

echo ""

# 4. Query data back
echo "─── Querying water_quality table ───"
echo ""
docker exec influxdb3 influxdb3 query \
  "SELECT * FROM water_quality ORDER BY time DESC LIMIT 10" \
  --database "$DATABASE" --host "$INFLUX_URL" 2>/dev/null

echo ""
echo "══════════════════════════════════════"
echo "  Entrypoints"
echo "══════════════════════════════════════"
echo ""
echo "  InfluxDB API  : http://localhost:8181"
echo "  Explorer UI   : http://localhost:8888"
echo "  Grafana       : http://localhost:3000  (admin/admin)"
echo ""
echo "  To view container logs:"
echo "    docker logs influxdb3"
echo "    docker logs influxdb3-explorer"
echo "    docker logs grafana"
echo ""

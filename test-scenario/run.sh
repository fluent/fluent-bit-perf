
rm -rf /tmp/test-sc*.log
rm -rf reports/*

FLB_PERF=/home/edsiper/c/fluent-bit-perf/build/bin/flb-tail-writer
DATA_FILE=performance_test_data.txt

RECORDS_PER_SECOND=5000
INCREASE_RECORDS=5000
SECONDS=60

OPTIONS="-i $INCREASE_RECORDS -r $RECORDS_PER_SECOND -s $SECONDS -F csv"

# Fluent Bit
# ----------

# Check that Fluent Bit is running
if ! pidof fluent-bit > /dev/null; then
  echo "Error: Fluent Bit is not running."
  exit 1
fi

echo "Testing Fluent Bit..."
# run with: fluent-bit -c test-scenario/fluent-bit-tail-tcp.yaml
$FLB_PERF -d $DATA_FILE -p `pidof fluent-bit` -o /tmp/test-scenario-fluentbit.log $OPTIONS > fluent-bit.csv


# Vector
# ------
echo "Testing Vector..."

# Check that Vector is running
if ! pidof vector > /dev/null; then
  echo "Error: Vector is not running."
  exit 1
fi

# run with: vector -c test-scenario/vector.toml
$FLB_PERF -d $DATA_FILE -p `pidof vector` -o /tmp/test-scenario-vector.log $OPTIONS  > vector.csv



# Fluentd
# -------
echo "Testing Fluentd..."
# run with  fluentd --no-supervisor -c test-scenario/fluentd.conf

# Check that Fluentd is running
FLUENTD_PID=`ps aux | grep fluentd | grep -v grep | awk '{print $2}'`

if [ -z "$FLUENTD_PID" ]; then
  echo "Error: Fluentd is not running."
  exit 1
fi

# Fluentd must be run with --no-supervisor option
$FLB_PERF -d $DATA_FILE -p $FLUENTD_PID -o /tmp/test-scenario-fluentd.log $OPTIONS > fluentd.csv

# OpenTelemetry Collector Contrib
# -------------------------------
echo "Testing OpenTelemetry Collector Contrib..."
# run with: otelcol-contrib -config test-scenario/otel-collector.yaml

# Check that OpenTelemetry Collector Contrib is running
if ! pidof otelcol-contrib > /dev/null; then
  echo "Error: OpenTelemetry Collector Contrib is not running."
  exit 1
fi

$FLB_PERF -d $DATA_FILE -p `pidof otelcol-contrib` -o /tmp/test-scenario-otel-contrib.log $OPTIONS -D 200 > otelcol-contrib.csv

echo "done."

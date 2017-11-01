#!/bin/bash
export MQTT_SERVER=$1
export MQTT_PORT=$2
echo $MQTT_SERVER:$MQTT_PORT
rm -rf build ; mkdir build ; cd build
cmake ..
make

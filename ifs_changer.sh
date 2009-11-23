#!/bin/bash

if [ $# \< 3 ]; then
	echo "Usage: $0 UPDATE_PERIOD_IN_SECOND IF_TO_BE_RANDOMIZED ..."
	exit 1
fi

update_period=$1
shift

while true; do
	start_from=`expr $RANDOM % 3`
	curr_addr=`expr 22 + $start_from`
	for interface in "$@"; do
		curr_addr=`expr 22 + $start_from`
		echo sudo ifconfig $interface 192.168.0.$curr_addr
		sudo ifconfig $interface 192.168.0.$curr_addr
		start_from=`expr \( $start_from + 1 \) % 3`
	done
	echo sleep $update_period
	sleep $update_period
done

exit 0

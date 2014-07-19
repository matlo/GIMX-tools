#!/bin/bash

if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

containsElement () { local e; for e in "${@:2}"; do [[ "$e" = "$1" ]] && return 0; done; return 1; }

echo "Unplug the bluetooth dongle, then press enter."

read

#retrieve all bluetooth addresses
declare -a PREV_ADDRESSES=($(hciconfig | grep "BD Address" | cut -f 3 -d ' '))

echo "Plug the bluetooth dongle."

DONGLE_ADDRESS=""

#wait for a new bluetooth address to appear
while [ "$DONGLE_ADDRESS" == "" ]
do
	declare -a ADDRESSES=($(hciconfig | grep "BD Address" | cut -f 3 -d ' ' | grep -v "00:00:00:00:00:00"))
	
	for i in ${ADDRESSES[*]}
	do
		containsElement "$i" "${PREV_ADDRESSES[@]}"
		if [ $? -eq 1 ]
		then
			DONGLE_ADDRESS=$i
			break
		fi
	done
	
	sleep 1
done

HCI=$(hciconfig | grep -B 1 $DONGLE_ADDRESS | head -n 1 | cut -f 1 -d ':')

if [ "$HCI" == "" ]
then
	echo Dongle not found!
	exit
fi

HCI_NUMBER=$(echo $HCI | sed 's/hci//')

echo "The bluetooth dongle address is $DONGLE_ADDRESS."
echo "The bluetooth hci number is $HCI_NUMBER."

#generate a link key for the DS4
DS4_LINK_KEY=$(date | md5sum | cut -f 1 -d ' ')

echo "Plug the DS4 with a USB cable."

#wait for a DS4 to appear
while true
do
	DS4_ADDRESS=$(ds4tool 2> /dev/null | grep "Current Bluetooth Device Address" | cut -f 5 -d ' ')
	
	if [ "$DS4_ADDRESS" != "" ]
	then
		break
	fi
	
	sleep 1
done

echo "The DS4 address is $DS4_ADDRESS."

#set the DS4 master
ds4tool -m $DONGLE_ADDRESS -l $DS4_LINK_KEY 2&> /dev/null

if [ $? -eq 255 ]
then
	echo "Failed to set DS4 master!" 1>&2
	exit
fi

echo "Unplug the DS4."

#wait for the DS4 to disappear
while true
do
	TEST_ADDRESS=$(ds4tool 2> /dev/null | grep "Current Bluetooth Device Address" | cut -f 5 -d ' ')
	
	if [ "$TEST_ADDRESS" == "" ]
	then
		break
	fi
	
	sleep 1
done

echo "Plug the teensy."

#wait for a teensy to appear
while true
do
	TEENSY_ADDRESS=$(ds4tool -t 2> /dev/null | grep "Current Bluetooth Device Address" | cut -f 5 -d ' ')
	
	if [ "$TEENSY_ADDRESS" != "" ]
	then
		break
	fi
	
	sleep 1
done

ds4tool -t -s $DONGLE_ADDRESS 2&> /dev/null
ds4tool -t -m 00:00:00:00:00:00 2&> /dev/null

if [ $? -eq 255 ]
then
	echo "Failed to set teensy addresses!" 1>&2
	exit
fi

echo "Unplug the teensy."

#wait for the Teensy to disappear
while true
do
	TEST_ADDRESS=$(ds4tool -t 2> /dev/null | grep "Current Bluetooth Device Address" | cut -f 5 -d ' ')
	
	if [ "$TEST_ADDRESS" == "" ]
	then
		break
	fi
	
	sleep 1
done

echo "Plug the teensy to the PS4, and wait a few seconds."

sleep 5

echo "Then plug the teensy back to the PC."

#wait for a link key
while true
do
	PS4_LINK_KEY=$(ds4tool -t 2> /dev/null | grep "Current link key" | cut -f 4 -d ' ')
	PS4_ADDRESS=$(ds4tool -t 2> /dev/null | grep "Current Bluetooth master" | cut -f 4 -d ' ')
	
	if [ "$PS4_LINK_KEY" != "" ] && [ "$PS4_LINK_KEY" != "00000000000000000000000000000000" ]
	then
		break
	fi
	
	sleep 1
done

if [ "$PS4_ADDRESS" == "" ]
then
	echo "Failed to retrive PS4 address!" 1>&2
	exit
fi

echo "The PS4 address is $PS4_ADDRESS."

echo "Everything was successful: setting dongle link keys."

LK_DIR=~/.gimx/bluetooth/"$DONGLE_ADDRESS"

mkdir -p $LK_DIR

#set dongle link key for the DS4
echo $DS4_ADDRESS $DS4_LINK_KEY 4 0 > $LK_DIR/linkkeys

#set dongle link key for the PS4
echo $PS4_ADDRESS $PS4_LINK_KEY 4 0 >> $LK_DIR/linkkeys

echo "To run gimx, type:"
echo "gimx -t DS4 -c config.xml -h $HCI_NUMBER -b $PS4_ADDRESS"

#!/bin/bash

if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

containsElement () { local e; for e in "${@:2}"; do [[ "$e" = "$1" ]] && return 0; done; return 1; }

echo "Unplug any Dualshock 4."
echo "Unplug any teensy."
echo "Unpug the bluetooth dongle."
echo "Then press enter."

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

echo "The bluetooth dongle address is: $DONGLE_ADDRESS."

#generate a link key for the DS4
DS4_LINK_KEY=$(date | md5sum | cut -f 1 -d ' ')

echo "Plug the DS4 with a USB cable."

DS4_ADDRESS=""

#wait for a DS4 to appear
while [ "$DS4_ADDRESS" == "" ]
do
	TEENSY_ADDRESS=$(ds4tool 2> /dev/null | grep -B 2 "Current link key" | grep "Current Bluetooth Device Address" | cut -f 5 -d ' ')
	
	if [ "$TEENSY_ADDRESS" != "" ]
	then
		echo "The teensy should have been unplugged!"
		exit
	fi
	
	DS4_ADDRESS=$(ds4tool 2> /dev/null | grep "Current Bluetooth Device Address" | cut -f 5 -d ' ')
	
	sleep 1
done

echo "The DS4 address is $DS4_ADDRESS"

#set the DS4 master
ds4tool -m $DONGLE_ADDRESS -l $DS4_LINK_KEY 2&> /dev/null

if [ $? -eq 255 ]
then
	echo "Failed to set DS4 master!" 1>&2
fi

echo "Unplug the DS4."

TEST_ADDRESS=""

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
	TEENSY_ADDRESS=$(ds4tool 2> /dev/null | grep -B 2 "Current link key" | grep "Current Bluetooth Device Address" | cut -f 5 -d ' ')
	
	if [ "$TEENSY_ADDRESS" != "" ]
	then
		break
	fi
	
	sleep 1
done

ds4tool -s $DONGLE_ADDRESS 2&> /dev/null

if [ $? -eq 255 ]
then
	echo "Failed to set teensy slave address!" 1>&2
	exit
fi

ds4tool -m 00:00:00:00:00:00 2&> /dev/null

if [ $? -eq 255 ]
then
	echo "Failed to set teensy master address!" 1>&2
	exit
fi

echo "Unplug the teensy."

TEST_ADDRESS=""

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

echo "Plug the teensy to the PS4, and wait a few seconds."

sleep 5

echo "Then plug the teensy back to the PC."

#wait for a link key
while true
do
	PS4_LINK_KEY=$(ds4tool 2> /dev/null | grep "Current link key" | cut -f 4 -d ' ')
	PS4_ADDRESS=$(ds4tool 2> /dev/null | grep "Current Bluetooth master" | cut -f 4 -d ' ')
	
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

echo "The PS4 address is $PS4_ADDRESS"

echo "Everything was successful: setting dongle link keys."

mkdir -p /var/lib/bluetooth/$DONGLE_ADDRESS

#set dongle link key for the DS4
sed "/$DS4_ADDRESS/d" -i /var/lib/bluetooth/$DONGLE_ADDRESS/linkkeys 2> /dev/null
echo $DS4_ADDRESS $DS4_LINK_KEY 4 0 >> /var/lib/bluetooth/$DONGLE_ADDRESS/linkkeys

#set dongle link key for the PS4
sed "/$PS4_ADDRESS/d" -i /var/lib/bluetooth/$DONGLE_ADDRESS/linkkeys 2> /dev/null
echo $PS4_ADDRESS $PS4_LINK_KEY 4 0 >> /var/lib/bluetooth/$DONGLE_ADDRESS/linkkeys

HCI=$(hciconfig | grep -B 1 $DONGLE_ADDRESS | head -n 1 | cut -f 1 -d ':')

if [ "$HCI" == "" ]
then
	echo Dongle not found!
	exit
fi

#stop the bluetooth service
service bluetooth stop 2&> /dev/null

#make sure the bluetooth dongle is up
hciconfig $HCI up pscan

hciconfig hci1 putkey $DS4_ADDRESS
hciconfig hci1 putkey $PS4_ADDRESS
hciconfig hci1 auth encrypt

HCI_NUMBER=$(echo $HCI | sed 's/hci//')

echo "To run gimx, type:"
echo "gimx -t DS4 -c config.xml --nograb -r 10 -h $HCI_NUMBER -b $PS4_ADDRESS"

#!/bin/bash

ARGC=$#
N=$1
MODE=$2
NZOMBIES=$(ps | grep cena | wc -l)

if test $NZOMBIES -eq 0; then
	make clean
	make

	if test $ARGC -lt 2; then
		./cena
		exit 1
	fi

	if test -e "/dev/shm/my_shm"; then
		rm "/dev/shm/my_shm"
	fi

	if test -x "./cena"; then
		echo "Lanciata l'applicazione" &
		./cena $N $MODE
	else
		echo >&2 "Eseguibile non trovato"
		exit 1
	fi
else
	echo >&2 "Ci sono processi zombie"
	exit 1
fi

exit 0

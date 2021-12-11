#!/usr/bin/zsh

CLIENTS=$1

echo "[*] Openning $CLIENTS concurrent clients"

for (( i  = 1; i <= $CLIENTS; i++ ))
do
./client requests.txt "client_$i" 127.0.0.1 9999 &
done
printf "Clients scripts finished"
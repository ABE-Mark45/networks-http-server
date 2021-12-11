#!/usr/bin/zsh

PORT=$1
CLIENTS=$2

echo "[*] Openning $CLIENTS concurrent clients"

for (( i  = 1; i <= $CLIENTS; i++ ))
do
./client requests.txt "client_$i" 127.0.0.1 $PORT &
done
printf "Clients scripts finished"
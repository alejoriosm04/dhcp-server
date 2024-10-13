#!/bin/bash

# Número de clientes que deseas ejecutar
NUM_CLIENTS=3  

if ! docker network ls | grep -q dhcp_net; then
  echo "Creando red de Docker..."
  sudo docker network create --subnet=192.168.1.0/24 dhcp_net
else
  echo "Red de Docker ya existe, usando dhcp_net."
fi

if docker ps | grep -q dhcp_server; then
  echo "El servidor DHCP ya está en ejecución."
else
  echo "Iniciando el servidor DHCP..."
  sudo docker run -d --name dhcp_server --net dhcp_net --ip 192.168.1.2 --cap-add=NET_ADMIN dhcp_server
fi

echo "Eliminando contenedores de clientes anteriores..."
for container in $(docker ps -a --filter "name=dhcp_client" --format "{{.ID}}"); do
  sudo docker rm -f $container
done

for ((i=1; i<=NUM_CLIENTS; i++))
do
  echo "Iniciando cliente DHCP $i..."

  # Abrir una nueva ventana de terminal usando xterm y ejecutar el cliente de manera interactiva
  xterm -hold -e "sudo docker run -it --name dhcp_client$i --net dhcp_net --cap-add=NET_ADMIN dhcp_client" &
done

echo "Todos los clientes DHCP están en ejecución."

docker logs -f dhcp_server

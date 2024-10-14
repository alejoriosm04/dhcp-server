#!/bin/bash

# Detener y eliminar contenedores existentes
sudo docker stop dhcp_server dhcp_relay dhcp_client1 dhcp_client2 dhcp_client3
sudo docker rm dhcp_server dhcp_relay dhcp_client1 dhcp_client2 dhcp_client3

# Eliminar redes existentes
sudo docker network rm dhcp_net_a dhcp_net_b

# Construir imágenes
sudo docker build -t dhcp_client -f Dockerfile.client .
sudo docker build -t dhcp_server -f Dockerfile.server . 
sudo docker build -t dhcp_relay -f Dockerfile.relay .

# Crear redes
sudo docker network create --subnet=192.168.1.0/24 dhcp_net_a
sudo docker network create --subnet=192.168.2.0/24 dhcp_net_b

sudo docker run --privileged -d dhcp_relay bash && container_id=$(sudo docker ps -lq) && sudo docker exec -it $container_id bash -c "echo 1 > /proc/sys/net/ipv4/ip_forward"


# Ejecutar servidor DHCP
xterm -hold -e "sudo docker run -it --name dhcp_server --net dhcp_net_b --ip 192.168.2.2 --cap-add=NET_ADMIN dhcp_server" &

# Ejecutar DHCP Relay
xterm -hold -e "sudo docker run -it --name dhcp_relay --net dhcp_net_a --ip 192.168.1.2 --cap-add=NET_ADMIN dhcp_relay" &

# Conectar DHCP Relay a la red B
sudo docker network connect dhcp_net_b dhcp_relay --ip 192.168.2.3

# Esperar a que los contenedores del servidor y relay estén listos
sleep 5

# Número de clientes que deseas ejecutar
NUM_CLIENTS=3

# Ejecutar clientes DHCP en ventanas separadas con xterm
for ((i=1; i<=NUM_CLIENTS; i++))
do
  echo "Iniciando cliente DHCP $i..."
  xterm -hold -e "sudo docker run -it --name dhcp_client$i --net dhcp_net_b --cap-add=NET_ADMIN dhcp_client" &
done

echo "Todos los clientes DHCP están en ejecución."

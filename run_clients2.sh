#!/bin/bash

# Detener y eliminar contenedores existentes
sudo docker stop dhcp_server dhcp_relay dhcp_client1 dhcp_client2
sudo docker rm dhcp_server dhcp_relay dhcp_client1 dhcp_client2

# Eliminar redes existentes
sudo docker network rm dhcp_net_a dhcp_net_b

sudo docker build -t dhcp_client -f Dockerfile.client .
sudo docker build -t dhcp_server -f Dockerfile.server .
sudo docker build -t dhcp_relay -f Dockerfile.relay .

# Crear redes
sudo docker network create --subnet=192.168.1.0/24 dhcp_net_a
sudo docker network create --subnet=192.168.2.0/24 dhcp_net_b

sudo docker run --privileged -d dhcp_relay bash && container_id=$(sudo docker ps -lq) && sudo docker exec -it $container_id bash -c "echo 1 > /proc/sys/net/ipv4/ip_forward"

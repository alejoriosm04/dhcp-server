#!/bin/bash

# Detener y eliminar contenedores existentes
sudo docker stop dhcp_server dhcp_relay dhcp_client1 dhcp_client2
sudo docker rm dhcp_server dhcp_relay dhcp_client1 dhcp_client2

# Eliminar redes existentes
sudo docker network rm dhcp_net subnet_b

# Crear redes
sudo docker network create --subnet=192.168.1.0/24 dhcp_net
sudo docker network create --subnet=192.168.2.0/24 subnet_b

# Ejecutar servidor DHCP
sudo docker run -d --name dhcp_server --net subnet_b --ip 192.168.2.2 --cap-add=NET_ADMIN dhcp_server

# Ejecutar DHCP Relay
sudo docker run -d --name dhcp_relay --net dhcp_net --ip 192.168.1.2 --cap-add=NET_ADMIN dhcp_relay
sudo docker network connect subnet_b dhcp_relay --ip 192.168.2.3

# Ejecutar clientes DHCP
sudo docker run -it --name dhcp_client1 --net dhcp_net --cap-add=NET_ADMIN dhcp_client
sudo docker run -it --name dhcp_client2 --net subnet_a --cap-add=NET_ADMIN dhcp_client

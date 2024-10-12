#!/bin/bash

echo "Deteniendo y eliminando contenedores de DHCP..."

# Detener todos los contenedores y eliminarlos
-------------------------------------------------------------------------------------------------------------------
sudo docker stop $(sudo docker ps -q)
sudo docker rm $(sudo docker ps -a -q)

echo "Todos los contenedore (server y cliente) DHCP han sido eliminados."

# Verificar si la red dhcp_net existe y eliminarla
if sudo docker network ls | grep -q dhcp_net; then
  echo "Eliminando la red dhcp_net..."
  sudo docker network rm dhcp_net
else
  echo "No se encontró la red dhcp_net."
fi

# Cerrar todas las ventanas de terminal abiertas por xterm
echo "Cerrando las ventanas de terminal abiertas..."
pkill xterm

echo "Limpieza completada."

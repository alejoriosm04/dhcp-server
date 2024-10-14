#!/bin/bash

echo "Deteniendo y eliminando contenedores de DHCP..."

# Detener todos los contenedores y eliminarlos
sudo docker stop $(sudo docker ps -q)
sudo docker rm $(sudo docker ps -a -q)

echo "Todos los contenedores (server y cliente) DHCP han sido eliminados."

# Verificar si las redes dhcp_net_a y dhcp_net_b existen y eliminarlas
if sudo docker network ls | grep -q dhcp_net_a; then
  echo "Eliminando la red dhcp_net_a..."
  sudo docker network rm dhcp_net_a
else
  echo "No se encontró la red dhcp_net_a."
fi

if sudo docker network ls | grep -q dhcp_net_b; then
  echo "Eliminando la red dhcp_net_b..."
  sudo docker network rm dhcp_net_b
else
  echo "No se encontró la red dhcp_net_b."
fi

# Cerrar todas las ventanas de terminal abiertas por xterm
echo "Cerrando las ventanas de terminal abiertas..."
pkill xterm

echo "Limpieza completada."

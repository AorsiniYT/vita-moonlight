#!/bin/bash

# Script para enviar el VPK de OSRS a PS Vita
# Lee la IP desde ip_vita.txt si PSVITAIP no está definido

if [ -z "$PSVITAIP" ]; then
    if [ -f "ip_vita.txt" ]; then
        PSVITAIP=$(cat ip_vita.txt | tr -d '\n')
    else
        echo "Error: Define PSVITAIP o crea ip_vita.txt con la IP"
        exit 1
    fi
fi

VPK_FILE="cmake-build-psv/moonlight_vita.vpk"

if [ ! -f "$VPK_FILE" ]; then
    echo "Error: No se encuentra $VPK_FILE. Compila primero con make."
    exit 1
fi

echo "Enviando VPK..."
curl --ftp-create-dirs -T "$VPK_FILE" ftp://${PSVITAIP}:1337/ux0:/ABM/moonlight_vita.vpk

    # 3. Lanzar la app
echo "[+] Enviando comando de lanzamiento..."
LAUNCH_RESPONSE=$(echo launch VITASHELL | nc "$PSVITAIP" 1338)
echo "[+] Respuesta de vitacompanion: $LAUNCH_RESPONSE"
if echo "$LAUNCH_RESPONSE" | grep -iq launched; then
    echo "[+] ¡Aplicación lanzada exitosamente!"
else
    echo "[+] Advertencia: No se recibió confirmación de lanzamiento."
fi
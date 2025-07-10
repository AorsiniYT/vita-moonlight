#!/bin/bash
# Script para descargar client.pem y key.pem de la Vita y verificar su validez

set -e

# Leer IP de ip_vita.txt
IP_FILE="$(dirname "$0")/ip_vita.txt"
if [ -f "$IP_FILE" ]; then
    VITA_IP=$(head -n 1 "$IP_FILE" | tr -d '\r\n')
else
    echo "[!] No se encontró ip_vita.txt. Edita el script y pon la IP manualmente."
    exit 1
fi

# Cambia esto por el nombre de tu host (carpeta en devices)
if [ -z "$1" ]; then
    echo "Uso: $0 <nombre_host>"
    echo "Ejemplo: $0 Aorsini-PC.local"
    exit 1
fi
HOST="$1"

VITA_PORT=1337
REMOTE_DIR="ux0:/data/moonlight/devices/$HOST"

# Descargar los archivos
for f in client.pem key.pem; do
    echo "[+] Descargando $f ..."
    curl --ftp-method nocwd "ftp://$VITA_IP:$VITA_PORT/$REMOTE_DIR/$f" -o "$f"
done

echo "[+] Archivos descargados."

# Verificar el certificado
if ! command -v openssl &>/dev/null; then
    echo "[!] openssl no está instalado. Instálalo para verificar los certificados."
    exit 1
fi

echo "\n[+] Información de client.pem:"
openssl x509 -in client.pem -text -noout

echo "\n[+] Verificando que la clave y el certificado coinciden:"
CERT_MOD=$(openssl x509 -noout -modulus -in client.pem | openssl md5)
KEY_MOD=$(openssl rsa -noout -modulus -in key.pem | openssl md5)
if [ "$CERT_MOD" = "$KEY_MOD" ]; then
    echo "[OK] La clave privada y el certificado coinciden."
else
    echo "[ERROR] La clave privada y el certificado NO coinciden."
fi

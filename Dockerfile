# Usamos una imagen de PostgreSQL (basada en Debian, en este caso bookworm es estándar)
FROM postgres:18-bookworm

# Actualizamos los repositorios e instalamos las dependencias exigidas
RUN apt-get update && apt-get install -y \
    gcc \
    make \
    postgresql-server-dev-18 \
    && rm -rf /var/lib/apt/lists/*

# Establecemos un directorio de trabajo donde montaremos el código de la Skip List
WORKDIR /app
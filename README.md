git clone https://github.com/JoacoRFM/BD2_Project.git
cd BD2_Project

docker compose up -d --build

docker compose ps

docker compose exec postgres psql -U bd2 -d bd2

CREATE EXTENSION IF NOT EXISTS skiplist;

CREATE TABLE alumnos (id INTEGER PRIMARY KEY, nombre TEXT, edad INTEGER);
INSERT INTO alumnos (id, nombre, edad) VALUES (1, 'Ana', 20), (2, 'Luis', 21), (3, 'Maria', 19);
SELECT * FROM alumnos;

SELECT sl_build('alumnos'::regclass, 'id');

SELECT sl_count();
SELECT sl_search(2);
SELECT * FROM alumnos WHERE ctid = sl_search(2);
SELECT * FROM sl_range(1, 3);

WITH nuevo AS (
    INSERT INTO alumnos (id, nombre, edad)
    VALUES (4, 'Pedro', 22)
    RETURNING id, ctid
)
SELECT sl_insert(id, ctid) FROM nuevo;

\q

COMANDOS DE TERMINAL

git pull origin main                         Descargar cambios recientes
docker compose up -d --build                 Iniciar y compilar el proyecto
docker compose ps                            Ver el estado de PostgreSQL
docker compose exec postgres psql -U bd2 -d bd2   Entrar a PostgreSQL
docker compose logs postgres                 Ver mensajes del servidor
docker compose restart postgres              Reiniciar PostgreSQL
docker compose down                          Detener sin borrar datos
docker compose down -v                       Detener y borrar datos guardados

COMANDOS DENTRO DE POSTGRESQL

\l                 Mostrar bases de datos
\c bd2             Conectarse a la base bd2
\dt                Mostrar tablas
\d alumnos         Mostrar estructura de alumnos
\dx                Mostrar extensiones
\?                 Ayuda de comandos de psql
\h                 Ayuda de SQL
\q                 Salir

COMANDOS SQL

CREATE DATABASE prueba;                                    Crear base de datos
CREATE TABLE alumnos (id INTEGER PRIMARY KEY, nombre TEXT, edad INTEGER);   Crear tabla
SELECT * FROM alumnos;                                     Ver filas
SELECT * FROM alumnos WHERE id = 2;                        Buscar por id
INSERT INTO alumnos (id, nombre, edad) VALUES (5, 'Sofia', 20);   Insertar fila
UPDATE alumnos SET edad = 23 WHERE id = 2;                 Actualizar fila
DELETE FROM alumnos WHERE id = 3;                          Eliminar fila
DROP TABLE alumnos;                                        Eliminar tabla

COMANDOS DE LA SKIP LIST

CREATE EXTENSION IF NOT EXISTS skiplist;      Activar extensión
SELECT sl_build('alumnos'::regclass, 'id');   Crear o reconstruir Skip List
SELECT sl_search(2);                         Buscar clave y obtener CTID
SELECT * FROM alumnos WHERE ctid = sl_search(2);   Obtener la fila encontrada
SELECT * FROM sl_range(1, 5);                Buscar CTID entre dos claves
SELECT sl_count();                           Contar claves
SELECT sl_clear();                           Vaciar Skip List
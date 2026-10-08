-- ====================================================================
-- SCRIPT DE DEMOSTRACIÓN Y BENCHMARK - PARCIAL BD II (CS272)
-- PostgreSQL 18.6 | Extensión: Skip List
-- ====================================================================

\timing on

-- 1. Habilitar la extensión desarrollada por el equipo
CREATE EXTENSION IF NOT EXISTS skiplist;

-- 2. Limpieza de tablas previas
DROP TABLE IF EXISTS bench_dataset CASCADE;

-- 3. Creación de tabla base con claves INTEGER
CREATE TABLE bench_dataset (
    id INT PRIMARY KEY,
    payload VARCHAR(100),
    created_at TIMESTAMP DEFAULT clock_timestamp()
);

-- ====================================================================
-- GENERACIÓN DE DATASETS (Rúbrica Sección 7.1)
-- ====================================================================
-- Parámetro: Cambiar 100000 por 500000 o 1000000 según la prueba
-- Semilla fijada para garantizar reproducibilidad exacta (Dataset D2)
SELECT setseed(0.42);

INSERT INTO bench_dataset (id, payload)
SELECT 
    (random() * 10000000)::INT AS id,
    'Registro de prueba #' || g
FROM generate_series(1, 100000) AS g
ON CONFLICT (id) DO NOTHING;

-- Verificación de tuplas y distribución física (CTID)
SELECT count(*) AS total_registros FROM bench_dataset;
SELECT ctid, id, payload FROM bench_dataset LIMIT 5;

-- ====================================================================
-- CREACIÓN Y REGISTRO DE ÍNDICES (Rúbrica Sección 6.2)
-- ====================================================================

-- A. Índice B-tree estándar de PostgreSQL (Referencia obligatoria)
DROP INDEX IF EXISTS idx_btree_bench;
CREATE INDEX idx_btree_bench ON bench_dataset USING btree (id);

-- B. Construcción del índice Skip List en memoria (C / PGXS)
SELECT skiplist_build('bench_dataset', 'id');

-- ====================================================================
-- WORKLOADS OBLIGATORIOS (Rúbrica Sección 7.2)
-- ====================================================================

-- --------------------------------------------------------------------
-- W1: Búsqueda exitosa por igualdad (Clave existente)
-- Comparación: Sin índice (Seq Scan) vs. B-tree vs. Skip List
-- --------------------------------------------------------------------
-- Caso 1: Acceso sin índice (forzando Seq Scan)
SET enable_indexscan = off;
SET enable_bitmapscan = off;
SELECT * FROM bench_dataset WHERE id = 1050;

-- Caso 2: B-tree de PostgreSQL
SET enable_indexscan = on;
SET enable_bitmapscan = on;
EXPLAIN ANALYZE SELECT * FROM bench_dataset WHERE id = 1050;

-- Caso 3: Skip List C Extension (Obtiene CTID y accede a tupla)
SELECT d.* FROM bench_dataset d
WHERE d.ctid = (SELECT skiplist_search(1050));

-- --------------------------------------------------------------------
-- W2: Búsqueda no exitosa (Clave inexistente en el dataset)
-- --------------------------------------------------------------------
-- Clave negativa garantizada ausente
SELECT * FROM bench_dataset WHERE id = -9999;
SELECT skiplist_search(-9999); -- Debe retornar NULL o indicador de no encontrado

-- --------------------------------------------------------------------
-- W3: Inserciones (10% adicional respecto al dataset inicial)
-- En dataset de 100k, se insertan 10k registros adicionales
-- --------------------------------------------------------------------
INSERT INTO bench_dataset (id, payload)
SELECT 
    (15000000 + g)::INT,
    'Nueva inserción dinámica #' || g
FROM generate_series(1, 10000) AS g;

-- Inserción directa en la Skip List de un registro nuevo para la demo
INSERT INTO bench_dataset (id, payload) VALUES (9999999, 'Demo en vivo') RETURNING ctid, id;
SELECT skiplist_insert(9999999, (SELECT ctid FROM bench_dataset WHERE id = 9999999));

-- Validar que la Skip List responde inmediatamente tras la inserción
SELECT * FROM bench_dataset WHERE ctid = (SELECT skiplist_search(9999999));

-- --------------------------------------------------------------------
-- W4: Búsqueda por rango (Soportada por la Skip List)
-- --------------------------------------------------------------------
-- B-tree
EXPLAIN ANALYZE 
SELECT * FROM bench_dataset WHERE id BETWEEN 2000000 AND 2050000;

-- Skip List por rango
SELECT d.id, d.payload 
FROM bench_dataset d
JOIN skiplist_range_search(2000000, 2050000) s ON d.ctid = s.result_ctid;

-- ====================================================================
-- DEPURACIÓN Y REVISIÓN EN VIVO (Preguntas de docente / Defensa)
-- ====================================================================
SELECT skiplist_validate();    -- Verifica invariantes y punteros forward
SELECT skiplist_debug_print(); -- Muestra niveles construidos en stdout/docker logs
-- HNSW索引和vector功能的全面回归测试

-- 清理现有测试表
DROP TABLE IF EXISTS test_hnsw_vector;

-- 测试1: 基本功能测试 - 3维向量
CREATE TABLE test_hnsw_vector (
    id SERIAL PRIMARY KEY,
    embedding vector(3)
);

-- 插入测试数据
INSERT INTO test_hnsw_vector (embedding)
SELECT
    ( 
        '[' || round((random() * 100)::numeric, 2) || ',' 
        || round((random() * 100)::numeric, 2) || ',' 
        || round((random() * 100)::numeric, 2) || ']'
    )::vector(3)
FROM generate_series(1, 1000);

-- 验证数据插入
SELECT count(*) AS total_rows FROM test_hnsw_vector;

-- 创建HNSW索引
CREATE INDEX IF NOT EXISTS idx_hnsw_embedding_l2
ON test_hnsw_vector
USING hnsw
(embedding vector_l2_ops);

-- 查看表结构
\d test_hnsw_vector;

-- 测试查询性能
EXPLAIN (COSTS OFF)
SELECT id, embedding <-> '[1.0, 2.0, 3.0]'::vector(3) AS distance
FROM test_hnsw_vector
ORDER BY embedding <-> '[1.0, 2.0, 3.0]'::vector(3)
LIMIT 5;

-- 验证距离计算
SELECT 
    '[1.0, 2.0, 3.0]'::vector(3) <-> '[4.0, 5.0, 6.0]'::vector(3) AS expected_l2_distance,
    '[1.0, 2.0, 3.0]'::vector(3) <=> '[4.0, 5.0, 6.0]'::vector(3) AS expected_cosine_distance
LIMIT 1;

-- 测试2: 余弦距离查询（使用L2索引，因为HNSW不支持余弦距离索引）
EXPLAIN (COSTS OFF)
SELECT id, embedding <=> '[1.0, 2.0, 3.0]'::vector(3) AS distance
FROM test_hnsw_vector
ORDER BY embedding <=> '[1.0, 2.0, 3.0]'::vector(3)
LIMIT 5;

-- 测试3: 高维向量测试
DROP TABLE IF EXISTS test_hnsw_vector_highdim;

CREATE TABLE test_hnsw_vector_highdim (
    id SERIAL PRIMARY KEY,
    embedding vector(100) -- 100维向量
);

-- 插入高维测试数据
INSERT INTO test_hnsw_vector_highdim (embedding)
SELECT
    ( 
        '[' || array_to_string(array_agg(round((random() * 100)::numeric, 2)), ',') || ']'
    )::vector(100)
FROM generate_series(1, 500) AS row_num, generate_series(1, 100) AS dim_num
GROUP BY row_num;

-- 创建高维向量索引
CREATE INDEX IF NOT EXISTS idx_hnsw_embedding_highdim
ON test_hnsw_vector_highdim
USING hnsw
(embedding vector_l2_ops);

-- 测试高维向量查询
EXPLAIN (COSTS OFF)
SELECT id, embedding <-> (SELECT embedding FROM test_hnsw_vector_highdim LIMIT 1) AS distance
FROM test_hnsw_vector_highdim
ORDER BY embedding <-> (SELECT embedding FROM test_hnsw_vector_highdim LIMIT 1)
LIMIT 10;

-- 测试4: 插入、更新、删除操作
INSERT INTO test_hnsw_vector (embedding) VALUES ('[1.0, 2.0, 3.0]'::vector(3));

UPDATE test_hnsw_vector SET embedding = '[4.0, 5.0, 6.0]'::vector(3) WHERE id = (SELECT max(id) FROM test_hnsw_vector);

DELETE FROM test_hnsw_vector WHERE id = (SELECT max(id) FROM test_hnsw_vector);

-- 测试5: 索引维护
VACUUM ANALYZE test_hnsw_vector;
VACUUM ANALYZE test_hnsw_vector_highdim;

-- 测试6: 重建索引
REINDEX INDEX idx_hnsw_embedding_l2;

-- 清理测试表
DROP TABLE IF EXISTS test_hnsw_vector;
DROP TABLE IF EXISTS test_hnsw_vector_highdim;


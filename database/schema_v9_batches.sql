-- ============================================
-- SCHEMA v9.0 - BATCHED DATA COLLECTION
-- ============================================
-- 
-- SOLUÇÃO: Fragmentar dados em lotes de 2000 amostras
-- Cada lote é uma linha separada, agrupados por session_uuid
--
-- Para reconstruir sessão completa:
-- SELECT * FROM hrv_batches 
-- WHERE session_uuid = 'xxx' 
-- ORDER BY batch_number;

CREATE TABLE IF NOT EXISTS hrv_batches (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    
    -- Agrupamento de sessão
    session_uuid TEXT NOT NULL,           -- UUID que agrupa todos os batches de uma sessão
    batch_number INT NOT NULL,            -- Número sequencial do batch (0, 1, 2...)
    
    -- Timing
    start_millis BIGINT,                  -- millis() do início deste batch
    end_millis BIGINT,                    -- millis() do fim deste batch
    sample_count INT,                     -- Quantidade de amostras neste batch
    
    -- Metadata
    device_id TEXT DEFAULT 'ESP32-S3-v9-Tanker',
    user_name TEXT DEFAULT 'Visitante',
    sampling_rate_hz INT DEFAULT 400,
    
    -- Raw Data (tamanho controlado: ~2000 amostras = ~14KB)
    ir_waveform JSONB,
    red_waveform JSONB
);

-- Índices para consultas eficientes
CREATE INDEX idx_hrv_batches_session_uuid ON hrv_batches(session_uuid);
CREATE INDEX idx_hrv_batches_created_at ON hrv_batches(created_at);

-- Comentário
COMMENT ON TABLE hrv_batches IS 'Batched PPG data from v9.0 Tanker. Each row = ~5 seconds of data.';

-- ============================================
-- VIEW para reconstruir sessões completas
-- ============================================
CREATE OR REPLACE VIEW hrv_sessions_complete AS
SELECT 
    session_uuid,
    MIN(created_at) as session_start,
    MAX(created_at) as session_end,
    COUNT(*) as total_batches,
    SUM(sample_count) as total_samples,
    MIN(batch_number) as first_batch,
    MAX(batch_number) as last_batch,
    MAX(user_name) as user_name,
    MAX(device_id) as device_id,
    MAX(sampling_rate_hz) as sampling_rate_hz
FROM hrv_batches
GROUP BY session_uuid;

COMMENT ON VIEW hrv_sessions_complete IS 'Aggregated view of complete sessions from batched data.';

-- Create a table for users (if not using Supabase Auth directly for linking)
-- For now, we assume simple anonymous or device-based logging.

CREATE TABLE IF NOT EXISTS hrv_sessions (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    
    -- Metadata
    device_id TEXT DEFAULT 'ESP32-S3',
    user_name TEXT DEFAULT 'Visitante', -- Name of the person being measured
    session_index INT, -- Matches the 'sessionNumber' on device
    timestamp_device_min INT, -- Minutes since boot
    sampling_rate_hz INT DEFAULT 200, -- Sampling rate in Hz (e.g. 200)
    
    -- Metrics (Computed on Device)
    fc_mean FLOAT,
    sdnn FLOAT,
    rmssd FLOAT,
    pnn50 FLOAT,
    rr_valid_count INT,
    
    -- Context
    tags TEXT[], -- Array of strings e.g. ['post-workout', 'resting']
    
    -- Raw Data (High Fidelity)
    -- Storing as JSONB arrays is flexible for Supabase
    rrr_intervals_ms JSONB, -- Array of RR intervals [800, 810, ...]
    
    -- Waveforms (Downsampled or Full)
    -- WARNING: 1 min @ 200Hz = 12000 points. 
    -- 3 channels = 36000 points.
    -- Arrays of Integers.
    ir_waveform JSONB,
    red_waveform JSONB,
    green_waveform JSONB
);

-- Index for faster querying by date
CREATE INDEX idx_hrv_sessions_created_at ON hrv_sessions(created_at);

-- Comment on table
COMMENT ON TABLE hrv_sessions IS 'Stores high-fidelity PPG data and HRV metrics from Pulse Analytics device.';

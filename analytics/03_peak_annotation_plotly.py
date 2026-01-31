# %% [markdown]
# # 🫀 Anotação de Picos PPG para Fine-tuning (Versão Plotly)
# 
# Esta versão usa Plotly para interatividade nativa no Jupyter.
# 
# ## Workflow:
# 1. Buscar sessões do Supabase
# 2. Selecionar sessão para anotar
# 3. Detecção automática de picos
# 4. Visualizar e revisar picos
# 5. Salvar anotações em CSV

# %% [markdown]
# ## 1. Instalar dependências

# %%
import sys
!{sys.executable} -m pip install plotly pandas numpy scipy sqlalchemy psycopg2-binary -q

# %% [markdown]
# ## 2. Imports e Configuração

# %%
import pandas as pd
import numpy as np
import plotly.graph_objects as go
import plotly.io as pio
from plotly.subplots import make_subplots
from scipy.signal import find_peaks
from sqlalchemy import create_engine
import os
from datetime import datetime

# Configurar renderer do Plotly para Jupyter
pio.renderers.default = "notebook"

# Configuração do Supabase
USER = 'postgres'
PASSWORD = '_xs#hiUAWeN6LMK'
HOST = 'db.pthfxmypcxqjfstqwokf.supabase.co'
PORT = '5432'
DBNAME = 'postgres'

url_conexao = f'postgresql://{USER}:{PASSWORD}@{HOST}:{PORT}/{DBNAME}'
engine = create_engine(url_conexao)

print("✅ Conexão configurada!")

# %% [markdown]
# ## 3. Carregar Sessões do Supabase

# %%
query = "SELECT * FROM hrv_sessions ORDER BY created_at DESC"
df = pd.read_sql(query, engine)

print(f"📊 {len(df)} sessões encontradas")
print("\nÚltimas 10 sessões:")
df[['id', 'created_at', 'device_id', 'user_name', 'sampling_rate_hz']].head(10)

# %% [markdown]
# ## 4. Selecionar Sessão

# %%
# Selecione o índice da sessão (0 = mais recente)
SESSAO_INDEX = 0  # ⬅️ ALTERE AQUI

sessao = df.iloc[SESSAO_INDEX]
print(f"📝 Sessão selecionada:")
print(f"   ID: {sessao['id']}")
print(f"   Device: {sessao['device_id']}")
print(f"   User: {sessao['user_name']}")
print(f"   Created: {sessao['created_at']}")
print(f"   Sampling Rate: {sessao['sampling_rate_hz']} Hz")
print(f"   IR samples: {len(sessao['ir_waveform'])}")

# %% [markdown]
# ## 5. Preprocessamento

# %%
def preprocess_ppg(signal):
    """Normaliza e inverte o sinal PPG"""
    signal = np.array(signal)
    sig_min = np.min(signal)
    sig_max = np.max(signal)
    sig_norm = (signal - sig_min) / (sig_max - sig_min)
    sig_inverted = 1.0 - sig_norm
    return sig_inverted

ir_signal = preprocess_ppg(sessao['ir_waveform'])
sampling_rate = sessao['sampling_rate_hz']

print(f"✅ Sinal preprocessado: {len(ir_signal)} amostras")
print(f"   Duração: {len(ir_signal) / sampling_rate:.1f} segundos")

# %% [markdown]
# ## 6. Detecção Automática de Picos

# %%
def detect_peaks_auto(signal, fs, min_hr=40, max_hr=200):
    """Detecta picos usando scipy.signal.find_peaks"""
    min_distance = int(fs * 60 / max_hr)
    peaks, _ = find_peaks(signal, distance=min_distance, height=0.3, prominence=0.1)
    return peaks

peaks_auto = detect_peaks_auto(ir_signal, sampling_rate)
print(f"🔍 {len(peaks_auto)} picos detectados automaticamente")

if len(peaks_auto) > 1:
    rr_intervals = np.diff(peaks_auto) / sampling_rate * 1000
    hr_mean = 60000 / np.mean(rr_intervals)
    print(f"❤️ HR médio estimado: {hr_mean:.1f} BPM")

# %% [markdown]
# ## 7. Visualização Interativa com Plotly
# 
# Use o zoom e pan para navegar pelo sinal. Os picos são marcados em vermelho.

# %%
def plot_ppg_plotly(signal, peaks, fs, title="PPG com Picos"):
    """Cria gráfico interativo com Plotly"""
    time = np.arange(len(signal)) / fs
    
    fig = go.Figure()
    
    # Sinal PPG
    fig.add_trace(go.Scatter(
        x=time,
        y=signal,
        mode='lines',
        name='PPG',
        line=dict(color='green', width=1)
    ))
    
    # Picos
    peak_times = peaks / fs
    peak_values = signal[peaks]
    fig.add_trace(go.Scatter(
        x=peak_times,
        y=peak_values,
        mode='markers',
        name='Picos',
        marker=dict(color='red', size=8, symbol='triangle-down')
    ))
    
    fig.update_layout(
        title=title,
        xaxis_title='Tempo (s)',
        yaxis_title='Amplitude',
        height=500,
        showlegend=True,
        hovermode='x unified'
    )
    
    # Adicionar range slider
    fig.update_xaxes(rangeslider_visible=True)
    
    return fig

fig = plot_ppg_plotly(ir_signal, peaks_auto, sampling_rate, 
                       f"Sessão {sessao['device_id']} - {len(peaks_auto)} picos")
fig.show()

# %% [markdown]
# ## 8. Revisar Picos por Janela
# 
# Visualize janela por janela para verificar se os picos estão corretos.

# %%
def plot_window(signal, peaks, fs, window_start, window_size=10):
    """Plota uma janela específica"""
    start_sample = int(window_start * fs)
    end_sample = int((window_start + window_size) * fs)
    end_sample = min(end_sample, len(signal))
    
    sig_window = signal[start_sample:end_sample]
    time = np.arange(len(sig_window)) / fs + window_start
    
    peaks_in_window = peaks[(peaks >= start_sample) & (peaks < end_sample)]
    peak_times = peaks_in_window / fs
    peak_values = signal[peaks_in_window]
    
    fig = go.Figure()
    fig.add_trace(go.Scatter(x=time, y=sig_window, mode='lines', name='PPG', line=dict(color='green')))
    fig.add_trace(go.Scatter(x=peak_times, y=peak_values, mode='markers', name='Picos', 
                              marker=dict(color='red', size=12, symbol='triangle-down')))
    
    fig.update_layout(
        title=f"Janela {window_start}s - {window_start + window_size}s | {len(peaks_in_window)} picos",
        xaxis_title='Tempo (s)',
        yaxis_title='Amplitude',
        height=400
    )
    fig.show()
    
    return peaks_in_window

# Visualizar primeira janela
total_duration = len(ir_signal) / sampling_rate
print(f"📊 Duração total: {total_duration:.1f}s")
print(f"   Janelas de 10s: {int(np.ceil(total_duration / 10))}")

# %%
# Altere WINDOW_START para ver diferentes partes do sinal
WINDOW_START = 0  # ⬅️ ALTERE AQUI (em segundos)
WINDOW_SIZE = 10

peaks_window = plot_window(ir_signal, peaks_auto, sampling_rate, WINDOW_START, WINDOW_SIZE)
print(f"Picos nesta janela: {peaks_window / sampling_rate}")

# %% [markdown]
# ## 9. Edição Manual de Picos (Opcional)
# 
# Se precisar remover ou adicionar picos, edite as listas abaixo:

# %%
# Copie os picos que deseja REMOVER (em segundos)
PICOS_REMOVER = []  # Ex: [2.5, 5.3, 10.1]

# Copie os picos que deseja ADICIONAR (em segundos)
PICOS_ADICIONAR = []  # Ex: [3.2, 7.8]

# Aplicar edições
peaks_final = peaks_auto.copy()

# Remover picos
for t in PICOS_REMOVER:
    sample = int(t * sampling_rate)
    # Encontrar pico mais próximo
    if len(peaks_final) > 0:
        idx = np.argmin(np.abs(peaks_final - sample))
        if np.abs(peaks_final[idx] - sample) < sampling_rate * 0.1:  # Dentro de 100ms
            peaks_final = np.delete(peaks_final, idx)
            print(f"➖ Pico removido em {t:.2f}s")

# Adicionar picos
for t in PICOS_ADICIONAR:
    sample = int(t * sampling_rate)
    # Snap para máximo local
    window = int(sampling_rate * 0.05)  # ±50ms
    start = max(0, sample - window)
    end = min(len(ir_signal), sample + window)
    local_max = start + np.argmax(ir_signal[start:end])
    
    if local_max not in peaks_final:
        peaks_final = np.sort(np.append(peaks_final, local_max))
        print(f"➕ Pico adicionado em {local_max / sampling_rate:.2f}s")

print(f"\n📊 Total de picos: {len(peaks_final)}")

# %% [markdown]
# ## 10. Salvar Anotações

# %%
def save_annotations(session_id, peaks, signal, sampling_rate, output_dir='./annotations'):
    """Salva anotações em CSV"""
    os.makedirs(output_dir, exist_ok=True)
    
    df_annotations = pd.DataFrame({
        'peak_index': peaks,
        'peak_time_s': peaks / sampling_rate,
        'peak_value': signal[peaks]
    })
    
    if len(peaks) > 1:
        rr_intervals = np.diff(peaks) / sampling_rate * 1000
        df_annotations['rr_interval_ms'] = np.concatenate([[np.nan], rr_intervals])
    
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    filename = f'{output_dir}/peaks_{session_id[:8]}_{timestamp}.csv'
    
    df_annotations.to_csv(filename, index=False)
    print(f"✅ Anotações salvas em: {filename}")
    
    return filename

# Salvar
output_file = save_annotations(
    session_id=str(sessao['id']),
    peaks=peaks_final,
    signal=ir_signal,
    sampling_rate=sampling_rate
)

# %% [markdown]
# ## 11. Resumo Final

# %%
print(f"📊 Resumo da Anotação:")
print(f"   Sessão: {sessao['id']}")
print(f"   Device: {sessao['device_id']}")
print(f"   Picos automáticos: {len(peaks_auto)}")
print(f"   Picos finais: {len(peaks_final)}")

if len(peaks_final) > 1:
    rr_intervals = np.diff(peaks_final) / sampling_rate * 1000
    hr_mean = 60000 / np.mean(rr_intervals)
    hr_std = np.std(60000 / rr_intervals)
    sdnn = np.std(rr_intervals)
    rmssd = np.sqrt(np.mean(np.diff(rr_intervals)**2))
    
    print(f"\n❤️ Métricas HRV:")
    print(f"   HR médio: {hr_mean:.1f} ± {hr_std:.1f} BPM")
    print(f"   SDNN: {sdnn:.1f} ms")
    print(f"   RMSSD: {rmssd:.1f} ms")

print(f"\n💾 Arquivo salvo: {output_file}")

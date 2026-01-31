# %% [markdown]
# # 🫀 Anotação de Picos PPG para Fine-tuning
# 
# Este notebook permite anotar picos sistólicos no sinal PPG coletado.
# 
# ## Workflow:
# 1. Buscar sessões do Supabase
# 2. Selecionar sessão para anotar
# 3. Detecção automática de picos
# 4. Correção manual (adicionar/remover)
# 5. Salvar anotações em CSV

# %% [markdown]
# ## 1. Imports e Configuração
# 
# **IMPORTANTE**: Execute a célula abaixo para instalar/habilitar o backend interativo!

# %%
# Instalar ipympl no ambiente correto do kernel (só precisa executar uma vez)
import sys
!{sys.executable} -m pip install ipympl -q

# %%
# Habilitar backend interativo
# Tente: %matplotlib widget OU %matplotlib notebook OU %matplotlib tk
%matplotlib notebook

# %%
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.widgets import Button
from scipy.signal import find_peaks
from sqlalchemy import create_engine
import os
from datetime import datetime

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
# ## 2. Carregar Sessões do Supabase

# %%
# Carregar todas as sessões
query = "SELECT * FROM hrv_sessions ORDER BY created_at DESC"
df = pd.read_sql(query, engine)

print(f"📊 {len(df)} sessões encontradas")
print("\nÚltimas 10 sessões:")
df[['id', 'created_at', 'device_id', 'user_name', 'sampling_rate_hz']].head(10)

# %% [markdown]
# ## 3. Selecionar Sessão para Anotar

# %%
# Selecione o índice da sessão (0 = mais recente)
SESSAO_INDEX = 0  # Altere aqui para anotar outra sessão

sessao = df.iloc[SESSAO_INDEX]
print(f"📝 Sessão selecionada:")
print(f"   ID: {sessao['id']}")
print(f"   Device: {sessao['device_id']}")
print(f"   User: {sessao['user_name']}")
print(f"   Created: {sessao['created_at']}")
print(f"   Sampling Rate: {sessao['sampling_rate_hz']} Hz")
print(f"   IR samples: {len(sessao['ir_waveform'])}")

# %% [markdown]
# ## 4. Preprocessamento do Sinal

# %%
def preprocess_ppg(signal):
    """
    Preprocessa o sinal PPG:
    1. Converte para numpy array
    2. Normaliza (0 a 1)
    3. Inverte (picos sistólicos para cima)
    """
    signal = np.array(signal)
    
    # Normalizar
    sig_min = np.min(signal)
    sig_max = np.max(signal)
    sig_norm = (signal - sig_min) / (sig_max - sig_min)
    
    # Inverter (sinal do sensor é invertido)
    sig_inverted = 1.0 - sig_norm
    
    return sig_inverted

# Preprocessar
ir_signal = preprocess_ppg(sessao['ir_waveform'])
sampling_rate = sessao['sampling_rate_hz']

print(f"✅ Sinal preprocessado: {len(ir_signal)} amostras")
print(f"   Duração: {len(ir_signal) / sampling_rate:.1f} segundos")

# %% [markdown]
# ## 5. Detecção Automática de Picos

# %%
def detect_peaks_auto(signal, fs, min_hr=40, max_hr=200):
    """
    Detecta picos automaticamente usando scipy.signal.find_peaks
    
    Args:
        signal: Sinal PPG (já invertido, picos para cima)
        fs: Taxa de amostragem em Hz
        min_hr: Frequência cardíaca mínima em BPM
        max_hr: Frequência cardíaca máxima em BPM
    
    Returns:
        Array com índices dos picos
    """
    # Calcular distância mínima entre picos
    # min_distance = tempo mínimo entre batimentos = 60/max_hr segundos
    min_distance = int(fs * 60 / max_hr)
    max_distance = int(fs * 60 / min_hr)
    
    # Detectar picos
    peaks, properties = find_peaks(
        signal,
        distance=min_distance,
        height=0.3,  # Altura mínima (0-1)
        prominence=0.1  # Proeminência mínima
    )
    
    return peaks

# Detectar picos
peaks_auto = detect_peaks_auto(ir_signal, sampling_rate)
print(f"🔍 {len(peaks_auto)} picos detectados automaticamente")

# Calcular HR médio
if len(peaks_auto) > 1:
    rr_intervals = np.diff(peaks_auto) / sampling_rate * 1000  # em ms
    hr_mean = 60000 / np.mean(rr_intervals)
    print(f"❤️ HR médio estimado: {hr_mean:.1f} BPM")

# %% [markdown]
# ## 6. Visualização dos Picos

# %%
def plot_peaks(signal, peaks, fs, window_start=0, window_size=10):
    """
    Plota o sinal com os picos marcados
    
    Args:
        signal: Sinal PPG
        peaks: Índices dos picos
        fs: Taxa de amostragem
        window_start: Início da janela em segundos
        window_size: Tamanho da janela em segundos
    """
    # Converter para samples
    start_sample = int(window_start * fs)
    end_sample = int((window_start + window_size) * fs)
    end_sample = min(end_sample, len(signal))
    
    # Extrair janela
    sig_window = signal[start_sample:end_sample]
    time = np.arange(len(sig_window)) / fs + window_start
    
    # Filtrar picos na janela
    peaks_in_window = peaks[(peaks >= start_sample) & (peaks < end_sample)]
    peak_times = peaks_in_window / fs
    peak_values = signal[peaks_in_window]
    
    # Plot
    fig, ax = plt.subplots(figsize=(15, 5))
    ax.plot(time, sig_window, 'g-', linewidth=0.8, label='PPG')
    ax.scatter(peak_times, peak_values, c='red', s=50, zorder=5, label='Picos')
    
    ax.set_xlabel('Tempo (s)')
    ax.set_ylabel('Amplitude (normalizada)')
    ax.set_title(f'PPG com Picos Detectados ({window_start}s - {window_start + window_size}s)')
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.show()
    
    return fig

# Visualizar primeira janela de 10 segundos
fig = plot_peaks(ir_signal, peaks_auto, sampling_rate, window_start=0, window_size=10)

# %% [markdown]
# ## 7. Interface de Anotação Manual

# %%
class PeakAnnotator:
    """
    Interface interativa para corrigir anotações de picos
    
    Clique esquerdo: Adicionar pico
    Clique direito: Remover pico mais próximo
    """
    
    def __init__(self, signal, peaks_initial, fs, window_size=10):
        self.signal = signal
        self.peaks = list(peaks_initial)
        self.fs = fs
        self.window_size = window_size
        self.current_window = 0
        self.total_windows = int(np.ceil(len(signal) / fs / window_size))
        
        # Setup figure
        self.fig, self.ax = plt.subplots(figsize=(15, 6))
        plt.subplots_adjust(bottom=0.2)
        
        # Botões de navegação
        ax_prev = plt.axes([0.2, 0.05, 0.1, 0.05])
        ax_next = plt.axes([0.4, 0.05, 0.1, 0.05])
        ax_save = plt.axes([0.6, 0.05, 0.1, 0.05])
        
        self.btn_prev = Button(ax_prev, '← Anterior')
        self.btn_next = Button(ax_next, 'Próxima →')
        self.btn_save = Button(ax_save, 'Salvar')
        
        self.btn_prev.on_clicked(self.prev_window)
        self.btn_next.on_clicked(self.next_window)
        self.btn_save.on_clicked(self.save_annotations)
        
        # Conectar eventos de clique
        self.cid = self.fig.canvas.mpl_connect('button_press_event', self.on_click)
        
        self.update_plot()
        
    def update_plot(self):
        self.ax.clear()
        
        # Calcular janela
        start_sample = int(self.current_window * self.window_size * self.fs)
        end_sample = int((self.current_window + 1) * self.window_size * self.fs)
        end_sample = min(end_sample, len(self.signal))
        
        # Extrair dados
        sig_window = self.signal[start_sample:end_sample]
        time = np.arange(len(sig_window)) / self.fs + self.current_window * self.window_size
        
        # Filtrar picos
        peaks_in_window = [p for p in self.peaks if start_sample <= p < end_sample]
        peak_times = [p / self.fs for p in peaks_in_window]
        peak_values = [self.signal[p] for p in peaks_in_window]
        
        # Plot
        self.ax.plot(time, sig_window, 'g-', linewidth=0.8)
        self.ax.scatter(peak_times, peak_values, c='red', s=80, zorder=5, marker='v')
        
        self.ax.set_xlabel('Tempo (s)')
        self.ax.set_ylabel('Amplitude')
        self.ax.set_title(
            f'Janela {self.current_window + 1}/{self.total_windows} | '
            f'{len(self.peaks)} picos | '
            f'Clique: Esquerdo=Adicionar, Direito=Remover'
        )
        self.ax.grid(True, alpha=0.3)
        
        self.fig.canvas.draw()
    
    def on_click(self, event):
        if event.inaxes != self.ax:
            return
        
        # Converter tempo para sample
        sample_clicked = int(event.xdata * self.fs)
        
        if event.button == 1:  # Clique esquerdo - adicionar
            # Encontrar máximo local próximo
            window = 50  # ±50 samples
            start = max(0, sample_clicked - window)
            end = min(len(self.signal), sample_clicked + window)
            local_max = start + np.argmax(self.signal[start:end])
            
            if local_max not in self.peaks:
                self.peaks.append(local_max)
                self.peaks.sort()
                print(f"➕ Pico adicionado em {local_max / self.fs:.2f}s")
            
        elif event.button == 3:  # Clique direito - remover
            if self.peaks:
                # Encontrar pico mais próximo
                distances = [abs(p - sample_clicked) for p in self.peaks]
                closest_idx = np.argmin(distances)
                
                if distances[closest_idx] < 100 * self.fs / 1000:  # Dentro de 100ms
                    removed = self.peaks.pop(closest_idx)
                    print(f"➖ Pico removido de {removed / self.fs:.2f}s")
        
        self.update_plot()
    
    def prev_window(self, event):
        if self.current_window > 0:
            self.current_window -= 1
            self.update_plot()
    
    def next_window(self, event):
        if self.current_window < self.total_windows - 1:
            self.current_window += 1
            self.update_plot()
    
    def save_annotations(self, event):
        self.fig.canvas.mpl_disconnect(self.cid)
        plt.close(self.fig)
        print(f"💾 Anotação finalizada com {len(self.peaks)} picos")
    
    def get_peaks(self):
        return np.array(sorted(self.peaks))

# %% [markdown]
# ## 8. Iniciar Anotação Interativa
# 
# Execute a célula abaixo para iniciar a anotação:
# - **Clique esquerdo**: Adicionar pico (snap para máximo local)
# - **Clique direito**: Remover pico mais próximo
# - **Botões**: Navegar entre janelas e salvar

# %%
# Iniciar anotador
annotator = PeakAnnotator(ir_signal, peaks_auto, sampling_rate, window_size=10)
plt.show()

# %% [markdown]
# ## 9. Salvar Anotações

# %%
# Obter picos finais após anotação
peaks_final = annotator.get_peaks()

print(f"📊 Resumo da Anotação:")
print(f"   Picos automáticos: {len(peaks_auto)}")
print(f"   Picos finais: {len(peaks_final)}")

# Calcular métricas
if len(peaks_final) > 1:
    rr_intervals = np.diff(peaks_final) / sampling_rate * 1000  # em ms
    hr_mean = 60000 / np.mean(rr_intervals)
    hr_std = np.std(60000 / rr_intervals)
    print(f"   HR médio: {hr_mean:.1f} ± {hr_std:.1f} BPM")
    print(f"   SDNN: {np.std(rr_intervals):.1f} ms")

# %%
# Salvar anotações em CSV
def save_annotations(session_id, peaks, signal, sampling_rate, output_dir='./annotations'):
    """
    Salva as anotações de picos em CSV
    """
    os.makedirs(output_dir, exist_ok=True)
    
    # Criar DataFrame com anotações
    df_annotations = pd.DataFrame({
        'peak_index': peaks,
        'peak_time_s': peaks / sampling_rate,
        'peak_value': signal[peaks]
    })
    
    # Calcular RR intervals
    if len(peaks) > 1:
        rr_intervals = np.diff(peaks) / sampling_rate * 1000
        df_annotations['rr_interval_ms'] = np.concatenate([[np.nan], rr_intervals])
    
    # Nome do arquivo
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    filename = f'{output_dir}/peaks_{session_id[:8]}_{timestamp}.csv'
    
    # Salvar
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
# ## 10. Visualização Final

# %%
# Plot final com todos os picos anotados
def plot_full_session(signal, peaks, fs, title="Sessão Completa"):
    """
    Plota a sessão completa com todos os picos
    """
    time = np.arange(len(signal)) / fs
    peak_times = peaks / fs
    peak_values = signal[peaks]
    
    fig, ax = plt.subplots(figsize=(20, 5))
    ax.plot(time, signal, 'g-', linewidth=0.5, alpha=0.7)
    ax.scatter(peak_times, peak_values, c='red', s=20, zorder=5)
    
    ax.set_xlabel('Tempo (s)')
    ax.set_ylabel('Amplitude')
    ax.set_title(f'{title} | {len(peaks)} picos anotados')
    ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.show()
    
    return fig

fig_final = plot_full_session(ir_signal, peaks_final, sampling_rate, 
                               title=f"Sessão {sessao['device_id']} - {sessao['user_name']}")

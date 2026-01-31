#!/usr/bin/env python3
"""
Ferramenta Interativa de Anotação de Picos PPG

Execute: python peak_annotator_app.py
Abre no navegador: http://localhost:8050

Clique: Adicionar/Remover pico
Botões: Salvar, Marcar como Ruim, Recarregar
"""

import dash
from dash import dcc, html, callback_context
from dash.dependencies import Input, Output, State
import plotly.graph_objects as go
import pandas as pd
import numpy as np
from scipy.signal import find_peaks
from sqlalchemy import create_engine
import json
from datetime import datetime
import os

# ============== CONFIGURAÇÃO ==============
USER = 'postgres'
PASSWORD = '_xs#hiUAWeN6LMK'
HOST = 'db.pthfxmypcxqjfstqwokf.supabase.co'
PORT = '5432'
DBNAME = 'postgres'

ANNOTATIONS_DIR = './annotations'
STATUS_FILE = './annotations/session_status.json'

url_conexao = f'postgresql://{USER}:{PASSWORD}@{HOST}:{PORT}/{DBNAME}'
engine = create_engine(url_conexao)

# ============== FUNÇÕES ==============
def load_session_status():
    """Carrega status das sessões (anotada/ruim/pendente)"""
    if os.path.exists(STATUS_FILE):
        with open(STATUS_FILE, 'r') as f:
            return json.load(f)
    return {}

def save_session_status(status_dict):
    """Salva status das sessões"""
    os.makedirs(ANNOTATIONS_DIR, exist_ok=True)
    with open(STATUS_FILE, 'w') as f:
        json.dump(status_dict, f, indent=2)

def get_session_label(session_id, status_dict):
    """Retorna emoji de status"""
    status = status_dict.get(session_id, 'pending')
    if status == 'done':
        return '✅'
    elif status == 'bad':
        return '❌'
    else:
        return '⏳'

def preprocess_ppg(signal):
    """Normaliza e inverte o sinal PPG"""
    signal = np.array(signal)
    sig_min = np.min(signal)
    sig_max = np.max(signal)
    sig_norm = (signal - sig_min) / (sig_max - sig_min)
    return 1.0 - sig_norm

def detect_peaks_auto(signal, fs, min_hr=40, max_hr=200):
    """Detecta picos automaticamente"""
    min_distance = int(fs * 60 / max_hr)
    peaks, _ = find_peaks(signal, distance=min_distance, height=0.3, prominence=0.1)
    return peaks.tolist()

def save_annotations(session_id, peaks, signal, sampling_rate):
    """Salva anotações em CSV"""
    os.makedirs(ANNOTATIONS_DIR, exist_ok=True)
    
    peaks = np.array(peaks)
    df_annotations = pd.DataFrame({
        'peak_index': peaks,
        'peak_time_s': peaks / sampling_rate,
        'peak_value': signal[peaks]
    })
    
    if len(peaks) > 1:
        rr_intervals = np.diff(peaks) / sampling_rate * 1000
        df_annotations['rr_interval_ms'] = np.concatenate([[np.nan], rr_intervals])
    
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    filename = f'{ANNOTATIONS_DIR}/peaks_{session_id[:8]}_{timestamp}.csv'
    df_annotations.to_csv(filename, index=False)
    
    return filename

# Carregar sessões e status
print("📊 Carregando sessões do Supabase...")
query = "SELECT * FROM hrv_sessions ORDER BY created_at DESC"
df_sessions = pd.read_sql(query, engine)
session_status = load_session_status()
print(f"✅ {len(df_sessions)} sessões encontradas")

# Contar status
done_count = sum(1 for s in session_status.values() if s == 'done')
bad_count = sum(1 for s in session_status.values() if s == 'bad')
pending_count = len(df_sessions) - done_count - bad_count
print(f"   ✅ Anotadas: {done_count} | ❌ Ruins: {bad_count} | ⏳ Pendentes: {pending_count}")

# ============== APP DASH ==============
app = dash.Dash(__name__)
app.title = "🫀 Anotador de Picos PPG"

# Estado global
current_signal = None
current_peaks = []
current_session = None
sampling_rate = 757

def create_dropdown_options():
    """Cria opções do dropdown com status"""
    status = load_session_status()
    options = []
    for i, row in df_sessions.iterrows():
        session_id = str(row['id'])
        emoji = get_session_label(session_id, status)
        label = f"{emoji} {row['device_id']} - {row['user_name']} ({row['created_at'].strftime('%Y-%m-%d %H:%M')})"
        options.append({'label': label, 'value': i})
    return options

app.layout = html.Div([
    html.H1("🫀 Anotador de Picos PPG", style={'textAlign': 'center'}),
    
    # Estatísticas
    html.Div(id='stats-display', style={
        'textAlign': 'center', 
        'margin': '10px',
        'padding': '10px',
        'backgroundColor': '#f0f0f0',
        'borderRadius': '5px'
    }),
    
    html.Div([
        html.Label("Selecione a sessão:"),
        dcc.Dropdown(
            id='session-dropdown',
            options=create_dropdown_options(),
            value=0,
            style={'width': '100%'}
        ),
    ], style={'margin': '20px', 'width': '60%'}),
    
    html.Div([
        html.Button('🔍 Detectar Picos', id='detect-btn', n_clicks=0, 
                    style={'margin': '5px', 'padding': '10px 20px'}),
        html.Button('💾 Salvar Anotações', id='save-btn', n_clicks=0,
                    style={'margin': '5px', 'padding': '10px 20px', 'backgroundColor': '#4CAF50', 'color': 'white'}),
        html.Button('❌ Marcar como Ruim', id='bad-btn', n_clicks=0,
                    style={'margin': '5px', 'padding': '10px 20px', 'backgroundColor': '#f44336', 'color': 'white'}),
        html.Button('⏭️ Próxima Pendente', id='next-btn', n_clicks=0,
                    style={'margin': '5px', 'padding': '10px 20px', 'backgroundColor': '#2196F3', 'color': 'white'}),
        html.Button('🔄 Recarregar', id='reload-btn', n_clicks=0,
                    style={'margin': '5px', 'padding': '10px 20px'}),
    ], style={'margin': '20px'}),
    
    html.Div([
        html.P("📌 Clique no gráfico para ADICIONAR pico | Clique em pico existente para REMOVER", 
               style={'fontWeight': 'bold', 'color': '#666'}),
    ]),
    
    dcc.Graph(
        id='ppg-graph',
        config={
            'displayModeBar': True,
            'scrollZoom': True,
        },
        style={'height': '55vh'}
    ),
    
    html.Div(id='status-text', style={'margin': '20px', 'fontSize': '16px'}),
    html.Div(id='save-status', style={'margin': '20px', 'fontSize': '16px', 'color': 'green'}),
    
    # Armazenamento de estado
    dcc.Store(id='peaks-store', data=[]),
    dcc.Store(id='signal-store', data=[]),
    dcc.Store(id='history-store', data=[]),
    dcc.Store(id='zoom-store', data={}),
], style={'fontFamily': 'Arial, sans-serif', 'maxWidth': '1400px', 'margin': '0 auto'})

@app.callback(
    Output('stats-display', 'children'),
    [Input('save-btn', 'n_clicks'),
     Input('bad-btn', 'n_clicks'),
     Input('reload-btn', 'n_clicks')]
)
def update_stats(save_clicks, bad_clicks, reload_clicks):
    status = load_session_status()
    done_count = sum(1 for s in status.values() if s == 'done')
    bad_count = sum(1 for s in status.values() if s == 'bad')
    pending_count = len(df_sessions) - done_count - bad_count
    
    return f"✅ Anotadas: {done_count} | ❌ Ruins: {bad_count} | ⏳ Pendentes: {pending_count} | 📊 Total: {len(df_sessions)}"

@app.callback(
    Output('session-dropdown', 'options'),
    [Input('save-btn', 'n_clicks'),
     Input('bad-btn', 'n_clicks'),
     Input('reload-btn', 'n_clicks')]
)
def update_dropdown(save_clicks, bad_clicks, reload_clicks):
    return create_dropdown_options()

@app.callback(
    Output('session-dropdown', 'value'),
    [Input('next-btn', 'n_clicks')],
    [State('session-dropdown', 'value')]
)
def next_pending(n_clicks, current_value):
    if n_clicks == 0:
        return current_value
    
    status = load_session_status()
    
    # Encontrar próxima pendente após a atual
    for i in range(current_value + 1, len(df_sessions)):
        session_id = str(df_sessions.iloc[i]['id'])
        if status.get(session_id, 'pending') == 'pending':
            return i
    
    # Se não encontrou, procurar desde o início
    for i in range(0, current_value):
        session_id = str(df_sessions.iloc[i]['id'])
        if status.get(session_id, 'pending') == 'pending':
            return i
    
    return current_value

@app.callback(
    [Output('ppg-graph', 'figure'),
     Output('peaks-store', 'data'),
     Output('signal-store', 'data'),
     Output('status-text', 'children')],
    [Input('session-dropdown', 'value'),
     Input('detect-btn', 'n_clicks'),
     Input('ppg-graph', 'clickData')],
    [State('peaks-store', 'data'),
     State('signal-store', 'data')]
)
def update_graph(session_idx, detect_clicks, click_data, peaks, signal):
    global current_signal, current_peaks, current_session, sampling_rate
    
    ctx = callback_context
    if not ctx.triggered:
        trigger = 'session-dropdown'
    else:
        trigger = ctx.triggered[0]['prop_id'].split('.')[0]
    
    # Determinar se é nova sessão (reset zoom) ou apenas edição (manter zoom)
    reset_view = False
    
    # Carregar nova sessão
    if trigger == 'session-dropdown' or signal is None or len(signal) == 0:
        session = df_sessions.iloc[session_idx]
        current_session = session
        sampling_rate = session['sampling_rate_hz']
        signal = preprocess_ppg(session['ir_waveform']).tolist()
        current_signal = np.array(signal)
        peaks = detect_peaks_auto(current_signal, sampling_rate)
        current_peaks = peaks
        reset_view = True  # Nova sessão, resetar zoom
    
    # Detectar picos
    if trigger == 'detect-btn':
        peaks = detect_peaks_auto(np.array(signal), sampling_rate)
        current_peaks = peaks
    
    # Adicionar/remover pico por clique
    if trigger == 'ppg-graph' and click_data:
        point = click_data['points'][0]
        click_time = point['x']
        click_sample = int(click_time * sampling_rate)
        
        sig_arr = np.array(signal)
        peaks_arr = np.array(peaks)
        
        if len(peaks_arr) > 0:
            distances = np.abs(peaks_arr - click_sample)
            min_dist = np.min(distances)
            closest_idx = np.argmin(distances)
            
            if min_dist < sampling_rate * 0.1:  # 100ms - remover
                peaks = [p for i, p in enumerate(peaks) if i != closest_idx]
            else:  # adicionar
                window = int(sampling_rate * 0.05)
                start = max(0, click_sample - window)
                end = min(len(sig_arr), click_sample + window)
                local_max = start + np.argmax(sig_arr[start:end])
                
                if local_max not in peaks:
                    peaks.append(local_max)
                    peaks.sort()
        else:
            window = int(sampling_rate * 0.05)
            start = max(0, click_sample - window)
            end = min(len(sig_arr), click_sample + window)
            local_max = start + np.argmax(sig_arr[start:end])
            peaks = [local_max]
        
        current_peaks = peaks
    
    # Criar gráfico
    sig_arr = np.array(signal)
    peaks_arr = np.array(peaks)
    time = np.arange(len(sig_arr)) / sampling_rate
    
    fig = go.Figure()
    
    fig.add_trace(go.Scattergl(
        x=time,
        y=sig_arr,
        mode='lines',
        name='PPG',
        line=dict(color='green', width=1)
    ))
    
    if len(peaks_arr) > 0:
        peak_times = peaks_arr / sampling_rate
        peak_values = sig_arr[peaks_arr]
        fig.add_trace(go.Scatter(
            x=peak_times,
            y=peak_values,
            mode='markers',
            name='Picos',
            marker=dict(color='red', size=10, symbol='triangle-down')
        ))
    
    # Status da sessão
    session_id = str(current_session['id'])
    status_dict = load_session_status()
    session_status_label = status_dict.get(session_id, 'pendente')
    
    # uirevision preserva zoom/pan quando o valor não muda
    # Muda apenas quando carrega nova sessão (reset_view = True)
    ui_revision = str(session_idx) if reset_view else "keep"
    
    fig.update_layout(
        title=f"Sessão: {current_session['device_id']} - {current_session['user_name']} | {len(peaks)} picos | Status: {session_status_label.upper()}",
        xaxis_title='Tempo (s)',
        yaxis_title='Amplitude',
        hovermode='x unified',
        xaxis=dict(rangeslider=dict(visible=True)),
        uirevision=ui_revision,  # Preserva zoom/pan
    )
    
    status = f"📊 {len(signal)} amostras | ⏱️ {len(signal)/sampling_rate:.1f}s | 🎯 {len(peaks)} picos"
    if len(peaks) > 1:
        rr = np.diff(peaks_arr) / sampling_rate * 1000
        hr = 60000 / np.mean(rr)
        status += f" | ❤️ {hr:.1f} BPM"
    
    return fig, peaks, signal, status

@app.callback(
    Output('save-status', 'children'),
    [Input('save-btn', 'n_clicks'),
     Input('bad-btn', 'n_clicks')],
    [State('peaks-store', 'data'),
     State('signal-store', 'data')]
)
def save_callback(save_clicks, bad_clicks, peaks, signal):
    ctx = callback_context
    if not ctx.triggered:
        return ""
    
    trigger = ctx.triggered[0]['prop_id'].split('.')[0]
    
    if current_session is None:
        return "❌ Nenhuma sessão carregada"
    
    session_id = str(current_session['id'])
    status_dict = load_session_status()
    
    if trigger == 'save-btn' and save_clicks > 0:
        filename = save_annotations(
            session_id,
            peaks,
            np.array(signal),
            sampling_rate
        )
        status_dict[session_id] = 'done'
        save_session_status(status_dict)
        return f"✅ Salvo em: {filename}"
    
    elif trigger == 'bad-btn' and bad_clicks > 0:
        status_dict[session_id] = 'bad'
        save_session_status(status_dict)
        return f"❌ Sessão marcada como RUIM"
    
    return ""

if __name__ == '__main__':
    print("\n" + "="*50)
    print("🫀 Anotador de Picos PPG")
    print("="*50)
    print("\nAbrindo no navegador: http://localhost:8050")
    print("\n📌 Instruções:")
    print("   - Clique no gráfico para ADICIONAR pico")
    print("   - Clique em pico existente para REMOVER")
    print("   - 💾 Salvar = marca como ANOTADA")
    print("   - ❌ Marcar como Ruim = descarta")
    print("   - ⏭️ Próxima Pendente = pula para próxima")
    print("\nPressione Ctrl+C para fechar")
    print("="*50 + "\n")
    
    app.run(debug=False, port=8050)

#!/bin/bash
# Script para executar o Anotador de Picos PPG

echo "🫀 Iniciando Anotador de Picos PPG..."
echo ""

# Matar processos anteriores na porta 8050
pkill -f peak_annotator_app.py 2>/dev/null
sleep 1

cd /home/douglas/Documentos/Projects/PPG/pulse-analytics/analytics

/home/douglas/miniconda3/bin/python peak_annotator_app.py

# Mantém o terminal aberto se houver erro
if [ $? -ne 0 ]; then
    echo ""
    echo "❌ Erro ao executar. Pressione Enter para fechar."
    read
fi

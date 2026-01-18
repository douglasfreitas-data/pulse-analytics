import { useState, useEffect } from 'react'
import { supabase } from './lib/supabase'
import { analyzeWaveform } from './lib/hrv-analysis'
import { MeuDia } from './components/MeuDia'

interface Session {
  id: string
  created_at: string
  session_index: number
  user_name: string
  sampling_rate_hz: number
  fc_mean: number | null
  sdnn: number | null
  rmssd: number | null
  pnn50: number | null
  device_id: string
  tags: string[] | null
  ir_waveform?: number[]
}

interface HRVMetrics {
  bpm: number
  sdnn: number
  rmssd: number
  pnn50: number
  rrIntervals: number[]
  peakIndices: number[]
}

interface SessionWithMetrics extends Session {
  calculatedMetrics?: HRVMetrics
  isAnalyzing?: boolean
}

type Tab = 'dashboard' | 'meudia' | 'checkup'

function App() {
  const [activeTab, setActiveTab] = useState<Tab>('meudia')
  const [sessions, setSessions] = useState<SessionWithMetrics[]>([])
  const [loading, setLoading] = useState(true)
  const [selectedSession, setSelectedSession] = useState<SessionWithMetrics | null>(null)

  useEffect(() => {
    if (activeTab === 'dashboard') {
      getSessions()
    }
  }, [activeTab])

  async function getSessions() {
    try {
      const { data, error } = await supabase
        .from('hrv_sessions')
        .select('id, created_at, session_index, user_name, sampling_rate_hz, fc_mean, sdnn, rmssd, pnn50, device_id, tags')
        .order('created_at', { ascending: false })
        .limit(20)

      if (error) throw error
      if (data) setSessions(data)
    } catch (error) {
      console.error('Error fetching sessions:', error)
    } finally {
      setLoading(false)
    }
  }

  async function analyzeSession(session: SessionWithMetrics) {
    if (session.calculatedMetrics) {
      setSelectedSession(session)
      return
    }

    setSelectedSession({ ...session, isAnalyzing: true })

    try {
      const { data, error } = await supabase
        .from('hrv_sessions')
        .select('ir_waveform, sampling_rate_hz')
        .eq('id', session.id)
        .single()

      if (error) throw error

      if (data?.ir_waveform && data.ir_waveform.length > 0) {
        const sampleRate = data.sampling_rate_hz || 800
        const metrics = analyzeWaveform(data.ir_waveform, sampleRate)

        const updatedSession = {
          ...session,
          calculatedMetrics: metrics,
          isAnalyzing: false
        }

        setSelectedSession(updatedSession)

        setSessions(prev => prev.map(s =>
          s.id === session.id ? updatedSession : s
        ))
      } else {
        setSelectedSession({ ...session, isAnalyzing: false })
      }
    } catch (error) {
      console.error('Error analyzing session:', error)
      setSelectedSession({ ...session, isAnalyzing: false })
    }
  }

  function getMetricValue(session: SessionWithMetrics, field: 'bpm' | 'sdnn' | 'rmssd' | 'pnn50') {
    if (session.calculatedMetrics) {
      const value = session.calculatedMetrics[field]
      return value > 0 ? value.toFixed(1) : '--'
    }
    const dbField = field === 'bpm' ? 'fc_mean' : field
    const dbValue = session[dbField as keyof Session]
    return typeof dbValue === 'number' ? dbValue.toFixed(1) : '--'
  }

  return (
    <div className="min-h-screen bg-[#0a0a0f] text-white font-sans">
      {/* Header com Navegação */}
      <header className="border-b border-gray-800 bg-gray-900/50 sticky top-0 z-40 backdrop-blur-sm">
        <div className="max-w-6xl mx-auto px-8 py-4">
          <div className="flex items-center justify-between">
            <div>
              <h1 className="text-2xl font-bold tracking-tight bg-gradient-to-r from-blue-400 to-cyan-400 bg-clip-text text-transparent">
                Pulse Analytics
              </h1>
              <p className="text-xs text-gray-500">Monitoramento Cardíaco</p>
            </div>

            {/* Navegação por Tabs */}
            <nav className="flex gap-2">
              <button
                onClick={() => setActiveTab('meudia')}
                className={`px-5 py-2.5 rounded-lg font-medium transition-all ${activeTab === 'meudia'
                  ? 'bg-green-500/20 text-green-400 border border-green-500/30'
                  : 'text-gray-400 hover:text-white hover:bg-gray-800'
                  }`}
              >
                💚 Meu Dia
              </button>
              <button
                onClick={() => setActiveTab('checkup')}
                className={`px-5 py-2.5 rounded-lg font-medium transition-all ${activeTab === 'checkup'
                  ? 'bg-purple-500/20 text-purple-400 border border-purple-500/30'
                  : 'text-gray-400 hover:text-white hover:bg-gray-800'
                  }`}
              >
                🔬 Check-up
              </button>
              <button
                onClick={() => setActiveTab('dashboard')}
                className={`px-5 py-2.5 rounded-lg font-medium transition-all ${activeTab === 'dashboard'
                  ? 'bg-blue-500/20 text-blue-400 border border-blue-500/30'
                  : 'text-gray-400 hover:text-white hover:bg-gray-800'
                  }`}
              >
                📊 Histórico
              </button>
            </nav>
          </div>
        </div>
      </header>

      {/* Conteúdo Principal */}
      <main className="max-w-6xl mx-auto px-8 py-8">
        {/* Tab: Meu Dia */}
        {activeTab === 'meudia' && <MeuDia />}

        {/* Tab: Check-up (placeholder) */}
        {activeTab === 'checkup' && (
          <div className="bg-gray-900/50 border border-gray-800 rounded-2xl p-8 text-center py-20">
            <div className="text-8xl mb-6">🔬</div>
            <h2 className="text-3xl font-bold mb-2">Check-up Completo</h2>
            <p className="text-gray-400 mb-4">
              Análise detalhada da saúde vascular + estresse
            </p>
            <p className="text-sm text-gray-500 bg-gray-800/50 inline-block px-4 py-2 rounded-lg">
              🚧 Em desenvolvimento • Firmware v15 @ 757 Hz
            </p>
          </div>
        )}

        {/* Tab: Dashboard (Histórico) */}
        {activeTab === 'dashboard' && (
          <>
            <div className="flex items-center justify-between mb-6">
              <h2 className="text-xl font-semibold">Histórico de Sessões</h2>
              <button
                onClick={() => getSessions()}
                className="px-4 py-2 bg-gray-800 text-gray-200 rounded-lg hover:bg-gray-700 transition border border-gray-700"
              >
                Atualizar
              </button>
            </div>

            {loading ? (
              <div className="text-center py-20 text-gray-400 animate-pulse">Carregando sessões...</div>
            ) : (
              <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
                {sessions.map(session => (
                  <div
                    key={session.id}
                    onClick={() => analyzeSession(session)}
                    className="bg-gray-900/50 border border-gray-800 rounded-xl p-6 shadow-lg hover:shadow-blue-500/10 hover:border-blue-500/30 transition-all cursor-pointer"
                  >
                    <div className="flex justify-between items-start mb-4">
                      <div>
                        <span className="text-xs font-mono text-gray-500 mb-1 block">#{session.session_index}</span>
                        <h3 className="font-semibold text-lg">{session.user_name || 'Visitante'}</h3>
                        {session.tags && session.tags.length > 0 && (
                          <span className="text-xs text-blue-400">{session.tags[0]}</span>
                        )}
                      </div>
                      <span className={`text-xs px-2 py-1 rounded-full font-medium ${session.sampling_rate_hz >= 700
                        ? 'bg-purple-500/20 text-purple-300'
                        : 'bg-green-500/20 text-green-300'
                        }`}>
                        {session.sampling_rate_hz}Hz
                      </span>
                    </div>

                    <div className="grid grid-cols-2 gap-4 mb-4">
                      <div>
                        <p className="text-xs text-gray-500">BPM Médio</p>
                        <p className="text-xl font-bold text-cyan-400">{getMetricValue(session, 'bpm')}</p>
                      </div>
                      <div>
                        <p className="text-xs text-gray-500">RMSSD</p>
                        <p className="text-xl font-bold text-green-400">{getMetricValue(session, 'rmssd')}</p>
                      </div>
                      <div>
                        <p className="text-xs text-gray-500">SDNN</p>
                        <p className="text-xl font-bold text-purple-400">{getMetricValue(session, 'sdnn')}</p>
                      </div>
                      <div>
                        <p className="text-xs text-gray-500">pNN50</p>
                        <p className="text-xl font-bold text-orange-400">{getMetricValue(session, 'pnn50')}%</p>
                      </div>
                    </div>

                    <div className="text-xs text-gray-500 border-t border-gray-800 pt-4 flex justify-between">
                      <span>{new Date(session.created_at).toLocaleString()}</span>
                      <span className="text-gray-600">{session.device_id?.split('-').pop()}</span>
                    </div>

                    {session.calculatedMetrics && (
                      <div className="mt-2 text-xs text-green-500/70 text-center">
                        ✓ {session.calculatedMetrics.rrIntervals.length} batimentos
                      </div>
                    )}
                  </div>
                ))}
              </div>
            )}
          </>
        )}

        {/* Modal de detalhes */}
        {selectedSession && (
          <div
            className="fixed inset-0 bg-black/80 flex items-center justify-center p-4 z-50"
            onClick={() => setSelectedSession(null)}
          >
            <div
              className="bg-gray-900 border border-gray-700 rounded-2xl p-8 max-w-lg w-full shadow-2xl"
              onClick={e => e.stopPropagation()}
            >
              <div className="flex justify-between items-start mb-6">
                <div>
                  <h2 className="text-2xl font-bold">{selectedSession.user_name || 'Visitante'}</h2>
                  <p className="text-gray-400">Sessão #{selectedSession.session_index}</p>
                </div>
                <button
                  onClick={() => setSelectedSession(null)}
                  className="text-gray-500 hover:text-white text-2xl"
                >
                  ×
                </button>
              </div>

              {selectedSession.isAnalyzing ? (
                <div className="text-center py-10">
                  <div className="animate-spin w-10 h-10 border-2 border-blue-500 border-t-transparent rounded-full mx-auto mb-4"></div>
                  <p className="text-gray-400">Analisando waveform...</p>
                </div>
              ) : (
                <>
                  <div className="grid grid-cols-2 gap-6 mb-6">
                    <div className="bg-gray-800/50 rounded-xl p-4 text-center">
                      <p className="text-xs text-gray-500 mb-1">BPM Médio</p>
                      <p className="text-4xl font-bold text-cyan-400">{getMetricValue(selectedSession, 'bpm')}</p>
                    </div>
                    <div className="bg-gray-800/50 rounded-xl p-4 text-center">
                      <p className="text-xs text-gray-500 mb-1">RMSSD (ms)</p>
                      <p className="text-4xl font-bold text-green-400">{getMetricValue(selectedSession, 'rmssd')}</p>
                    </div>
                    <div className="bg-gray-800/50 rounded-xl p-4 text-center">
                      <p className="text-xs text-gray-500 mb-1">SDNN (ms)</p>
                      <p className="text-4xl font-bold text-purple-400">{getMetricValue(selectedSession, 'sdnn')}</p>
                    </div>
                    <div className="bg-gray-800/50 rounded-xl p-4 text-center">
                      <p className="text-xs text-gray-500 mb-1">pNN50</p>
                      <p className="text-4xl font-bold text-orange-400">{getMetricValue(selectedSession, 'pnn50')}%</p>
                    </div>
                  </div>

                  {selectedSession.calculatedMetrics && (
                    <div className="bg-gray-800/30 rounded-xl p-4 mb-6">
                      <p className="text-xs text-gray-500 mb-2">Análise</p>
                      <div className="flex justify-between text-sm">
                        <span className="text-gray-400">Batimentos detectados:</span>
                        <span className="text-white font-medium">{selectedSession.calculatedMetrics.peakIndices.length}</span>
                      </div>
                      <div className="flex justify-between text-sm">
                        <span className="text-gray-400">Intervalos RR válidos:</span>
                        <span className="text-white font-medium">{selectedSession.calculatedMetrics.rrIntervals.length}</span>
                      </div>
                      <div className="flex justify-between text-sm">
                        <span className="text-gray-400">Taxa de amostragem:</span>
                        <span className="text-white font-medium">{selectedSession.sampling_rate_hz} Hz</span>
                      </div>
                    </div>
                  )}

                  <div className="text-xs text-gray-500 text-center">
                    {new Date(selectedSession.created_at).toLocaleString()} • {selectedSession.device_id}
                  </div>
                </>
              )}
            </div>
          </div>
        )}
      </main>
    </div>
  )
}

export default App

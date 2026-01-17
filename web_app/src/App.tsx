import { useState, useEffect } from 'react'
import { supabase } from './lib/supabase'

interface Session {
  id: string
  created_at: string
  session_index: number
  user_name: string
  sampling_rate_hz: number
  fc_mean: number
  sdnn: number
  rmssd: number
  pnn50: number
  device_id: string
}

function App() {
  const [sessions, setSessions] = useState<Session[]>([])
  const [loading, setLoading] = useState(true)

  useEffect(() => {
    getSessions()
  }, [])

  async function getSessions() {
    try {
      const { data, error } = await supabase
        .from('hrv_sessions')
        .select('*')
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

  return (
    <div className="min-h-screen bg-background text-foreground p-8 font-sans">
      <div className="max-w-6xl mx-auto">
        <header className="mb-8 flex items-center justify-between">
          <div>
            <h1 className="text-3xl font-bold tracking-tight text-primary">Pulse Analytics</h1>
            <p className="text-muted-foreground">Dashboard de Monitoramento Cardíaco</p>
          </div>
          <button
            onClick={() => getSessions()}
            className="px-4 py-2 bg-secondary text-secondary-foreground rounded-lg hover:bg-secondary/80 transition"
          >
            Atualizar
          </button>
        </header>

        {loading ? (
          <div className="text-center py-20 text-muted-foreground animate-pulse">Carregando sessões...</div>
        ) : (
          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
            {sessions.map(session => (
              <div key={session.id} className="bg-card border border-border rounded-xl p-6 shadow-sm hover:shadow-md transition-shadow">
                <div className="flex justify-between items-start mb-4">
                  <div>
                    <span className="text-xs font-mono text-muted-foreground mb-1 block">#{session.session_index}</span>
                    <h3 className="font-semibold text-lg">{session.user_name || 'Visitante'}</h3>
                  </div>
                  <span className="text-xs bg-accent text-accent-foreground px-2 py-1 rounded-full font-medium">
                    {session.sampling_rate_hz}Hz
                  </span>
                </div>

                <div className="grid grid-cols-2 gap-4 mb-4">
                  <div>
                    <p className="text-xs text-muted-foreground">BPM Médio</p>
                    <p className="text-xl font-bold">{session.fc_mean?.toFixed(1) || '--'}</p>
                  </div>
                  <div>
                    <p className="text-xs text-muted-foreground">RMSSD</p>
                    <p className="text-xl font-bold">{session.rmssd?.toFixed(1) || '--'}</p>
                  </div>
                  <div>
                    <p className="text-xs text-muted-foreground">SDNN</p>
                    <p className="text-xl font-bold">{session.sdnn?.toFixed(1) || '--'}</p>
                  </div>
                  <div>
                    <p className="text-xs text-muted-foreground">pNN50</p>
                    <p className="text-xl font-bold">{session.pnn50?.toFixed(1) || '--'}%</p>
                  </div>
                </div>

                <div className="text-xs text-muted-foreground border-t border-border pt-4 flex justify-between">
                  <span>{new Date(session.created_at).toLocaleString()}</span>
                  <span>{session.device_id}</span>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  )
}

export default App

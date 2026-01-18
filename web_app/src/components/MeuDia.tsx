import { useState, useEffect, useCallback } from 'react';
import {
    isSerialSupported,
    connectSerial,
    disconnectSerial,
    onSerialOutput,
    PulseCommands,
} from '../lib/serial';

interface MeasurementState {
    status: 'idle' | 'connecting' | 'ready' | 'collecting' | 'uploading' | 'done' | 'error';
    progress: number; // 0-100
    timeRemaining: string; // "04:32"
    samples: number;
    rate: number; // Hz
    message: string;
}

export function MeuDia() {
    const [serialSupported] = useState(isSerialSupported());
    const [connected, setConnected] = useState(false);
    const [logs, setLogs] = useState<string[]>([]);
    const [measurement, setMeasurement] = useState<MeasurementState>({
        status: 'idle',
        progress: 0,
        timeRemaining: '05:00',
        samples: 0,
        rate: 0,
        message: '',
    });

    // Configuração do usuário
    const [userName, setUserName] = useState('');
    const [userAge, setUserAge] = useState('');
    const [userSex, setUserSex] = useState<'M' | 'F' | ''>('');
    const [sessionTag, setSessionTag] = useState('');

    // Processar output do ESP32
    const handleSerialOutput = useCallback((line: string) => {
        setLogs(prev => [...prev.slice(-50), line]); // Manter últimas 50 linhas

        // Parser do output do ESP32
        if (line.includes('COLETA INICIADA') || line.includes('MEU DIA')) {
            setMeasurement(prev => ({
                ...prev,
                status: 'collecting',
                message: 'Coletando dados...',
            }));
        } else if (line.includes('COLETA CONCLUIDA') || line.includes('PRONTO!')) {
            setMeasurement(prev => ({
                ...prev,
                status: 'uploading',
                message: 'Enviando dados...',
            }));
        } else if (line.includes('Upload OK') || line.includes('SUCESSO')) {
            setMeasurement(prev => ({
                ...prev,
                status: 'done',
                progress: 100,
                message: 'Medição concluída com sucesso!',
            }));
        } else if (line.includes('FALHA') || line.includes('ERRO')) {
            setMeasurement(prev => ({
                ...prev,
                status: 'error',
                message: line,
            }));
        } else if (line.match(/\[\d+s\]/)) {
            // Parse de progresso: "[30s] Amostras: 6000 | Taxa: 200 Hz"
            const match = line.match(/\[(\d+)s\].*Amostras:\s*(\d+).*Taxa:\s*(\d+)/);
            if (match) {
                const elapsed = parseInt(match[1]);
                const samples = parseInt(match[2]);
                const rate = parseInt(match[3]);
                const totalSeconds = 300; // 5 minutos
                const remaining = totalSeconds - elapsed;
                const minutes = Math.floor(remaining / 60);
                const seconds = remaining % 60;

                setMeasurement(prev => ({
                    ...prev,
                    progress: (elapsed / totalSeconds) * 100,
                    timeRemaining: `${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`,
                    samples,
                    rate,
                }));
            }
        } else if (line.includes('IR:') || line.includes('Red:')) {
            // Progresso do upload
            const match = line.match(/(\d+)%/);
            if (match) {
                setMeasurement(prev => ({
                    ...prev,
                    message: `Enviando... ${match[1]}%`,
                }));
            }
        }
    }, []);

    // Registrar callback de output
    useEffect(() => {
        onSerialOutput(handleSerialOutput);
    }, [handleSerialOutput]);

    // Conectar ao dispositivo
    const handleConnect = async () => {
        setMeasurement(prev => ({ ...prev, status: 'connecting' }));
        const success = await connectSerial();
        setConnected(success);
        if (success) {
            setMeasurement(prev => ({
                ...prev,
                status: 'ready',
                message: 'Dispositivo conectado!',
            }));
            // Solicitar status
            setTimeout(() => PulseCommands.status(), 500);
        } else {
            setMeasurement(prev => ({
                ...prev,
                status: 'error',
                message: 'Falha ao conectar. Verifique o dispositivo.',
            }));
        }
    };

    // Desconectar
    const handleDisconnect = async () => {
        await disconnectSerial();
        setConnected(false);
        setMeasurement({
            status: 'idle',
            progress: 0,
            timeRemaining: '05:00',
            samples: 0,
            rate: 0,
            message: '',
        });
    };

    // Iniciar medição
    const handleStart = async () => {
        if (!connected) return;

        // Enviar configurações primeiro
        if (userName) await PulseCommands.setUser(userName);
        if (userAge) await PulseCommands.setAge(parseInt(userAge));
        if (userSex) await PulseCommands.setSex(userSex);
        if (sessionTag) await PulseCommands.setTag(sessionTag);

        // Pequeno delay para processar
        await new Promise(r => setTimeout(r, 100));

        // Iniciar coleta
        await PulseCommands.start();

        setMeasurement(prev => ({
            ...prev,
            status: 'collecting',
            progress: 0,
            timeRemaining: '05:00',
            samples: 0,
            message: 'Posicione o dedo no sensor...',
        }));
    };

    // Renderização condicional por status
    const renderContent = () => {
        if (!serialSupported) {
            return (
                <div className="text-center py-20">
                    <div className="text-6xl mb-4">🚫</div>
                    <h2 className="text-2xl font-bold text-red-400 mb-2">Navegador Não Suportado</h2>
                    <p className="text-gray-400">
                        Web Serial API não está disponível neste navegador.
                        <br />
                        Use Chrome, Edge ou Opera para conectar via USB.
                    </p>
                </div>
            );
        }

        if (measurement.status === 'idle' || measurement.status === 'connecting') {
            return (
                <div className="text-center py-20">
                    <div className="text-8xl mb-6">💚</div>
                    <h2 className="text-3xl font-bold mb-2">Meu Dia</h2>
                    <p className="text-gray-400 mb-8">
                        Avalie seu estresse, recuperação e equilíbrio do sistema nervoso
                    </p>
                    <button
                        onClick={handleConnect}
                        disabled={measurement.status === 'connecting'}
                        className="px-8 py-4 bg-gradient-to-r from-green-500 to-emerald-600 rounded-xl text-xl font-semibold hover:from-green-600 hover:to-emerald-700 transition-all disabled:opacity-50"
                    >
                        {measurement.status === 'connecting' ? 'Conectando...' : 'Conectar Dispositivo'}
                    </button>
                    <p className="text-xs text-gray-500 mt-4">
                        Conecte o ESP32 via USB e clique no botão acima
                    </p>
                </div>
            );
        }

        if (measurement.status === 'ready') {
            return (
                <div className="py-10">
                    <div className="text-center mb-8">
                        <div className="text-6xl mb-4">✅</div>
                        <h2 className="text-2xl font-bold text-green-400">Dispositivo Conectado</h2>
                        <p className="text-gray-400">Configure seus dados e inicie a medição</p>
                    </div>

                    {/* Form de configuração */}
                    <div className="max-w-md mx-auto space-y-4 mb-8">
                        <div>
                            <label className="block text-xs text-gray-500 mb-1">Nome (opcional)</label>
                            <input
                                type="text"
                                value={userName}
                                onChange={e => setUserName(e.target.value)}
                                placeholder="Seu nome"
                                className="w-full bg-gray-800 border border-gray-700 rounded-lg px-4 py-2 focus:border-green-500 focus:outline-none"
                            />
                        </div>

                        <div className="grid grid-cols-2 gap-4">
                            <div>
                                <label className="block text-xs text-gray-500 mb-1">Idade</label>
                                <input
                                    type="number"
                                    value={userAge}
                                    onChange={e => setUserAge(e.target.value)}
                                    placeholder="Ex: 30"
                                    className="w-full bg-gray-800 border border-gray-700 rounded-lg px-4 py-2 focus:border-green-500 focus:outline-none"
                                />
                            </div>
                            <div>
                                <label className="block text-xs text-gray-500 mb-1">Sexo</label>
                                <select
                                    value={userSex}
                                    onChange={e => setUserSex(e.target.value as 'M' | 'F' | '')}
                                    className="w-full bg-gray-800 border border-gray-700 rounded-lg px-4 py-2 focus:border-green-500 focus:outline-none"
                                >
                                    <option value="">Selecione</option>
                                    <option value="M">Masculino</option>
                                    <option value="F">Feminino</option>
                                </select>
                            </div>
                        </div>

                        <div>
                            <label className="block text-xs text-gray-500 mb-1">Tag (opcional)</label>
                            <input
                                type="text"
                                value={sessionTag}
                                onChange={e => setSessionTag(e.target.value)}
                                placeholder="Ex: manhã, pós-treino, estressado..."
                                className="w-full bg-gray-800 border border-gray-700 rounded-lg px-4 py-2 focus:border-green-500 focus:outline-none"
                            />
                        </div>
                    </div>

                    <div className="text-center">
                        <button
                            onClick={handleStart}
                            className="px-12 py-5 bg-gradient-to-r from-green-500 to-emerald-600 rounded-2xl text-2xl font-bold hover:from-green-600 hover:to-emerald-700 transition-all shadow-lg shadow-green-500/30"
                        >
                            ▶️ Iniciar Medição
                        </button>
                        <p className="text-xs text-gray-500 mt-4">
                            Duração: 5 minutos • Mantenha o dedo no sensor
                        </p>

                        <button
                            onClick={handleDisconnect}
                            className="mt-6 text-gray-500 hover:text-gray-300 text-sm"
                        >
                            Desconectar dispositivo
                        </button>
                    </div>
                </div>
            );
        }

        if (measurement.status === 'collecting') {
            return (
                <div className="py-10 text-center">
                    {/* Timer grande */}
                    <div className="text-8xl font-bold font-mono text-green-400 mb-4">
                        {measurement.timeRemaining}
                    </div>

                    {/* Barra de progresso */}
                    <div className="max-w-md mx-auto mb-6">
                        <div className="h-3 bg-gray-800 rounded-full overflow-hidden">
                            <div
                                className="h-full bg-gradient-to-r from-green-500 to-emerald-400 transition-all duration-1000"
                                style={{ width: `${measurement.progress}%` }}
                            />
                        </div>
                    </div>

                    {/* Stats ao vivo */}
                    <div className="flex justify-center gap-8 mb-8">
                        <div>
                            <p className="text-xs text-gray-500">Amostras</p>
                            <p className="text-2xl font-bold text-cyan-400">{measurement.samples.toLocaleString()}</p>
                        </div>
                        <div>
                            <p className="text-xs text-gray-500">Taxa</p>
                            <p className="text-2xl font-bold text-purple-400">{measurement.rate} Hz</p>
                        </div>
                    </div>

                    <p className="text-gray-400">{measurement.message}</p>

                    {/* Animação de pulso */}
                    <div className="mt-8">
                        <div className="w-24 h-24 mx-auto rounded-full bg-green-500/20 animate-pulse flex items-center justify-center">
                            <div className="w-16 h-16 rounded-full bg-green-500/40 animate-ping" />
                        </div>
                    </div>
                </div>
            );
        }

        if (measurement.status === 'uploading') {
            return (
                <div className="py-20 text-center">
                    <div className="text-8xl mb-6">☁️</div>
                    <h2 className="text-2xl font-bold mb-2">Enviando Dados</h2>
                    <p className="text-gray-400 mb-8">{measurement.message}</p>
                    <div className="animate-spin w-12 h-12 border-4 border-blue-500 border-t-transparent rounded-full mx-auto" />
                </div>
            );
        }

        if (measurement.status === 'done') {
            return (
                <div className="py-20 text-center">
                    <div className="text-8xl mb-6">✅</div>
                    <h2 className="text-3xl font-bold text-green-400 mb-2">Medição Concluída!</h2>
                    <p className="text-gray-400 mb-8">
                        {measurement.samples.toLocaleString()} amostras coletadas
                    </p>
                    <button
                        onClick={() =>
                            setMeasurement({
                                status: 'ready',
                                progress: 0,
                                timeRemaining: '05:00',
                                samples: 0,
                                rate: 0,
                                message: '',
                            })
                        }
                        className="px-8 py-4 bg-gradient-to-r from-green-500 to-emerald-600 rounded-xl text-xl font-semibold hover:from-green-600 hover:to-emerald-700 transition-all"
                    >
                        Nova Medição
                    </button>
                </div>
            );
        }

        if (measurement.status === 'error') {
            return (
                <div className="py-20 text-center">
                    <div className="text-8xl mb-6">❌</div>
                    <h2 className="text-2xl font-bold text-red-400 mb-2">Erro</h2>
                    <p className="text-gray-400 mb-8">{measurement.message}</p>
                    <button
                        onClick={() =>
                            setMeasurement({
                                status: connected ? 'ready' : 'idle',
                                progress: 0,
                                timeRemaining: '05:00',
                                samples: 0,
                                rate: 0,
                                message: '',
                            })
                        }
                        className="px-8 py-4 bg-gray-700 rounded-xl text-xl font-semibold hover:bg-gray-600 transition-all"
                    >
                        Tentar Novamente
                    </button>
                </div>
            );
        }

        return null;
    };

    return (
        <div className="bg-gray-900/50 border border-gray-800 rounded-2xl p-8">
            {renderContent()}

            {/* Terminal de logs (debug) */}
            {connected && logs.length > 0 && (
                <details className="mt-8">
                    <summary className="text-xs text-gray-500 cursor-pointer hover:text-gray-400">
                        Logs do dispositivo ({logs.length})
                    </summary>
                    <pre className="mt-2 text-xs text-gray-600 bg-black/50 rounded-lg p-4 max-h-40 overflow-auto font-mono">
                        {logs.slice(-20).join('\n')}
                    </pre>
                </details>
            )}
        </div>
    );
}

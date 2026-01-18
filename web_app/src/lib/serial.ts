/**
 * Web Serial API para comunicação com ESP32
 * Permite iniciar medições diretamente do navegador
 */

export interface SerialConnection {
    port: SerialPort | null;
    reader: ReadableStreamDefaultReader<string> | null;
    writer: WritableStreamDefaultWriter<string> | null;
}

let connection: SerialConnection = {
    port: null,
    reader: null,
    writer: null,
};

let outputCallback: ((line: string) => void) | null = null;

/**
 * Verifica se o navegador suporta Web Serial API
 */
export function isSerialSupported(): boolean {
    return 'serial' in navigator;
}

/**
 * Conecta ao dispositivo ESP32 via USB
 */
export async function connectSerial(): Promise<boolean> {
    if (!isSerialSupported()) {
        console.error('Web Serial API não suportada neste navegador');
        return false;
    }

    try {
        // Solicitar permissão do usuário para selecionar porta
        const port = await navigator.serial.requestPort({
            filters: [
                { usbVendorId: 0x303a }, // Espressif (ESP32-S3)
                { usbVendorId: 0x10c4 }, // Silicon Labs (CP2102)
                { usbVendorId: 0x1a86 }, // CH340
            ]
        });

        await port.open({ baudRate: 115200 });

        connection.port = port;

        // Configurar reader
        const textDecoder = new TextDecoderStream();
        const readableStream = port.readable as unknown as ReadableStream<Uint8Array>;
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        readableStream.pipeTo(textDecoder.writable as any);
        connection.reader = textDecoder.readable.getReader();

        // Configurar writer
        const textEncoder = new TextEncoderStream();
        const writableStream = port.writable as unknown as WritableStream<Uint8Array>;
        textEncoder.readable.pipeTo(writableStream);
        connection.writer = textEncoder.writable.getWriter();

        // Iniciar leitura em background
        readLoop();

        console.log('Conectado ao ESP32!');
        return true;
    } catch (error) {
        console.error('Erro ao conectar:', error);
        return false;
    }
}

/**
 * Desconecta do dispositivo
 */
export async function disconnectSerial(): Promise<void> {
    if (connection.reader) {
        await connection.reader.cancel();
        connection.reader = null;
    }
    if (connection.writer) {
        await connection.writer.close();
        connection.writer = null;
    }
    if (connection.port) {
        await connection.port.close();
        connection.port = null;
    }
    console.log('Desconectado');
}

/**
 * Verifica se está conectado
 */
export function isConnected(): boolean {
    return connection.port !== null;
}

/**
 * Envia comando para o ESP32
 */
export async function sendCommand(command: string): Promise<void> {
    if (!connection.writer) {
        throw new Error('Não conectado');
    }
    await connection.writer.write(command + '\n');
    console.log(`Enviado: ${command}`);
}

/**
 * Registra callback para receber output do ESP32
 */
export function onSerialOutput(callback: (line: string) => void): void {
    outputCallback = callback;
}

/**
 * Loop de leitura do serial
 */
async function readLoop(): Promise<void> {
    if (!connection.reader) return;

    let buffer = '';

    try {
        while (true) {
            const { value, done } = await connection.reader.read();
            if (done) break;

            buffer += value;

            // Processar linhas completas
            const lines = buffer.split('\n');
            buffer = lines.pop() || ''; // Última linha incompleta volta pro buffer

            for (const line of lines) {
                const trimmed = line.trim();
                if (trimmed && outputCallback) {
                    outputCallback(trimmed);
                }
            }
        }
    } catch (error) {
        console.error('Erro na leitura serial:', error);
    }
}

/**
 * Comandos de alto nível para o Pulse Analytics
 */
export const PulseCommands = {
    /** Inicia a coleta de dados */
    start: () => sendCommand('start'),

    /** Reenvia dados após falha */
    retry: () => sendCommand('retry'),

    /** Define o nome do usuário */
    setUser: (name: string) => sendCommand(`USER:${name}`),

    /** Define a tag da sessão */
    setTag: (tag: string) => sendCommand(`TAG:${tag}`),

    /** Define a idade do usuário */
    setAge: (age: number) => sendCommand(`AGE:${age}`),

    /** Define o sexo do usuário */
    setSex: (sex: 'M' | 'F') => sendCommand(`SEX:${sex}`),

    /** Solicita status do dispositivo */
    status: () => sendCommand('status'),

    /** Exibe ajuda */
    help: () => sendCommand('help'),
};

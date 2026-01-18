/**
 * Web Serial API type declarations
 * https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API
 */

interface SerialPortInfo {
    usbVendorId?: number;
    usbProductId?: number;
}

interface SerialPortRequestOptions {
    filters?: { usbVendorId?: number; usbProductId?: number }[];
}

interface SerialOptions {
    baudRate: number;
    dataBits?: number;
    stopBits?: number;
    parity?: 'none' | 'even' | 'odd';
    bufferSize?: number;
    flowControl?: 'none' | 'hardware';
}

interface SerialPort {
    readable: ReadableStream<Uint8Array> | null;
    writable: WritableStream<Uint8Array> | null;
    open(options: SerialOptions): Promise<void>;
    close(): Promise<void>;
    getInfo(): SerialPortInfo;
}

interface Serial extends EventTarget {
    getPorts(): Promise<SerialPort[]>;
    requestPort(options?: SerialPortRequestOptions): Promise<SerialPort>;
}

interface Navigator {
    serial: Serial;
}

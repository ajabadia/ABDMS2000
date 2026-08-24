// IPC Bridge between WebUI and C++ / WASM

export class BridgeCore {
  constructor() {
    this.isWebView = typeof window !== 'undefined' && window.__JUCE__ !== undefined;
    this.listeners = new Map();

    if (this.isWebView && window.__JUCE__.backend) {
      window.__JUCE__.backend.addEventListener('event', (data) => {
        this.emitLocal(data.type, data.data);
      });
    }
  }

  send(action, payload = {}) {
    const msg = { action, ...payload };
    if (this.isWebView && window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent(msg);
    } else {
      console.log('[Bridge Simulated Native Call]:', msg);
    }
  }

  setParam(paramId, value) {
    this.send('setParam', { paramId, value });
  }

  noteOn(note, velocity = 0.8) {
    this.send('noteOn', { note, velocity });
  }

  noteOff(note, velocity = 0.0) {
    this.send('noteOff', { note, velocity });
  }

  allNotesOff() {
    this.send('allNotesOff');
  }

  on(event, callback) {
    if (!this.listeners.has(event)) {
      this.listeners.set(event, new Set());
    }
    this.listeners.get(event).add(callback);
  }

  off(event, callback) {
    if (this.listeners.has(event)) {
      this.listeners.get(event).delete(callback);
    }
  }

  emitLocal(event, data) {
    if (this.listeners.has(event)) {
      this.listeners.get(event).forEach(cb => cb(data));
    }
  }
}

export const bridge = new BridgeCore();

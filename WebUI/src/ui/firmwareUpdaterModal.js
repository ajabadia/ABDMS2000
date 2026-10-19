/**
 * KORG MS2000 / MS2000R Hardware Firmware Updater (v1.07) & 1 MB Flash Backup Modal.
 *
 * Uses the embedded official Korg x811v107.sys (256 KB, SHA-1 945a7555...) and
 * unknown-technologies' IPL Command 0x01 Flash Dump patch in C++ (MS2000IplUpdater).
 */

import { bridge } from '../bridge/bridgeCore.js';
import { createOverlayFocus } from '@abdsynths/shared/components';

class FirmwareUpdaterModalController {
  constructor() {
    this.modal = null;
    this.focusContract = null;
    this.confirmed = false;
    this.busy = false;
    this.unitDetected = false;
    this.autoPollEnabled = true;
    this.autoPollTimer = null;
    this.inputs = [];
    this.outputs = [];
    this.selectedInputId = '';
    this.selectedOutputId = '';
    this.transferRate = 0; // 0: Very Fast, 1: Fast, 2: Slow, 3: Very Slow
    this.embeddedInfo = {
      embeddedValid: true,
      v107Sha1: '945a75555fcd3f17d7ad9f15d38a431869a07bb8',
      dumpSysSha1: 'cd85d579969411b8cd55a6ccd83690247b51c7ca',
      sizeBytes: 262144,
    };
  }

  init() {
    this.modal = document.getElementById('firmware-updater-modal');
    if (!this.modal) return;

    this.render();

    this.focusContract = createOverlayFocus({
      root: this.modal,
      isClosed: () => this.modal.classList.contains('hidden'),
      onEscape: () => {
        if (!this.busy) this.close();
      },
    });
    this.focusContract.setInert();
    this.focusContract.attach();

    this.bindDomEvents();
    this.bindBridgeEvents();
  }

  render() {
    this.modal.innerHTML = `
      <div class="modal-box diagnostic-modal-box ipl-modal-box" style="max-width: 860px; width: 95%;">
        <div class="diag-header">
          <div class="diag-title">
            <span class="diag-badge-icon" style="background: rgba(56, 189, 248, 0.16); color: #38bdf8;">
              <svg class="ui-icon" viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                <rect x="4" y="4" width="16" height="16" rx="2" ry="2"></rect>
                <rect x="9" y="9" width="6" height="6"></rect>
                <line x1="9" y1="1" x2="9" y2="4"></line><line x1="15" y1="1" x2="15" y2="4"></line>
                <line x1="9" y1="20" x2="9" y2="23"></line><line x1="15" y1="20" x2="15" y2="23"></line>
                <line x1="20" y1="9" x2="23" y2="9"></line><line x1="20" y1="14" x2="23" y2="14"></line>
                <line x1="1" y1="9" x2="4" y2="9"></line><line x1="1" y1="14" x2="4" y2="14"></line>
              </svg>
            </span>
            <h3>KORG MS2000 / MS2000R &mdash; HARDWARE FIRMWARE UPDATER v1.07</h3>
          </div>
          <button id="ipl-close-top-btn" class="diag-icon-btn" title="Close modal">✕</button>
        </div>
        <div class="modal-sub">
          Embedded Official KORG System OS v1.07 (<code>x811v107.sys</code> &bull; 256 KB &bull; SHA-1 Verified) &amp; 1 MB Full Flash Backup Utility
        </div>

        <div class="diag-body" style="gap: 12px;">
          <!-- 0. Critical Hardware Warning & Confirmation -->
          <div class="diag-card" style="border-color: rgba(248, 113, 113, 0.45); background: rgba(127, 29, 29, 0.18);">
            <div style="display: flex; align-items: flex-start; gap: 10px;">
              <span style="color: #f87171; font-size: 1.2rem; line-height: 1;">&#9888;</span>
              <div style="flex: 1;">
                <div style="color: #fca5a5; font-weight: 700; font-size: 0.82rem; letter-spacing: 0.03em; margin-bottom: 4px;">
                  CRITICAL COMPATIBILITY WARNING: ONLY FOR KORG MS2000 OR MS2000R (ORIGINAL MODEL)
                </div>
                <div style="color: #e2e8f0; font-size: 0.76rem; line-height: 1.45;">
                  <strong>NEVER use this utility with a KORG MS2000B or MS2000BR.</strong> The &ldquo;B&rdquo; models use different hardware and firmware &mdash; installing this image on an MS2000B/BR can render the unit unusable. Back up any custom user programs before updating.
                </div>
                <label style="display: flex; align-items: center; gap: 8px; margin-top: 8px; cursor: pointer; font-size: 0.78rem; color: #fef08a; font-weight: 600;">
                  <input type="checkbox" id="ipl-confirm-checkbox" style="width: 16px; height: 16px; accent-color: #38bdf8;">
                  <span>I confirm my unit is an original KORG MS2000 or MS2000R (NO &ldquo;B&rdquo; in its model name) and I accept the firmware update procedure.</span>
                </label>
              </div>
            </div>
          </div>

          <!-- 1. Instructions to Enter IPL Update Receive Mode -->
          <div class="diag-card">
            <div class="diag-card-title">1. HOW TO PUT YOUR MS2000 / MS2000R INTO UPDATE RECEIVE MODE (IPL)</div>
            <div style="display: grid; grid-template-columns: 1.35fr 1fr; gap: 14px; align-items: center;">
              <ol style="margin: 0; padding-left: 18px; color: #cbd5e1; font-size: 0.77rem; line-height: 1.55;">
                <li>Switch the <strong>MS2000 / MS2000R power OFF</strong>.</li>
                <li>Connect <strong>two MIDI cables</strong> (bidirectional SysEx handshake is required):
                  <br>&bull; Computer <strong>MIDI OUT &rarr; MS2000 MIDI IN</strong>
                  <br>&bull; MS2000 <strong>MIDI OUT &rarr; Computer MIDI IN</strong>
                </li>
                <li>Hold down both the <strong>WRITE</strong> button and the Arpeggiator <strong>TYPE</strong> button simultaneously, and switch the unit <strong>ON</strong>.</li>
                <li>Release both buttons when the hardware LCD displays <code>IPL s.p.u [    ]</code>.</li>
              </ol>

              <!-- Simulated 16x2 Hardware LCD + Live State Capture -->
              <div style="display: flex; flex-direction: column; gap: 8px; align-items: stretch;">
                <div style="background: #090d14; border: 1px solid #334155; border-radius: 6px; padding: 10px 12px; box-shadow: inset 0 2px 6px rgba(0,0,0,0.7);">
                  <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 6px;">
                    <span style="font-size: 0.65rem; color: #64748b; font-weight: 700; letter-spacing: 0.08em;">HARDWARE IPL LCD MONITOR</span>
                    <span id="ipl-state-badge" style="font-size: 0.66rem; padding: 2px 7px; border-radius: 10px; font-weight: 700; background: rgba(148, 163, 184, 0.15); color: #94a3b8;">
                      STANDBY
                    </span>
                  </div>
                  <div style="background: #1a2e1a; border: 1px solid #2d4c2d; border-radius: 4px; padding: 8px 12px; font-family: 'Courier New', monospace; font-size: 1.05rem; font-weight: 700; letter-spacing: 0.12em; color: #7cfc00; text-shadow: 0 0 6px rgba(124, 252, 0, 0.45); line-height: 1.35;">
                    <div id="ipl-lcd-line1" style="white-space: pre;">IPL s.p.u [    ]</div>
                    <div id="ipl-lcd-line2" style="white-space: pre;">................</div>
                  </div>
                  <div id="ipl-version-pills" style="display: flex; justify-content: space-between; gap: 6px; margin-top: 8px; font-size: 0.71rem; color: #94a3b8;">
                    <span>System: <strong id="ipl-ver-sys" style="color: #e2e8f0;">&mdash;</strong></span>
                    <span>PCM: <strong id="ipl-ver-pcm" style="color: #e2e8f0;">&mdash;</strong></span>
                    <span>User: <strong id="ipl-ver-usr" style="color: #e2e8f0;">&mdash;</strong></span>
                  </div>
                </div>
              </div>
            </div>
          </div>

          <!-- 2. MIDI Ports & Transfer Speed -->
          <div class="diag-card">
            <div class="diag-card-title">2. MIDI INTERFACE &amp; TRANSFER RATE CONFIGURATION</div>
            <div style="display: grid; grid-template-columns: 1fr 1fr 0.85fr auto; gap: 10px; align-items: end;">
              <div>
                <label style="display: block; font-size: 0.72rem; color: #94a3b8; margin-bottom: 4px;">MIDI IN Port (from MS2000 MIDI OUT):</label>
                <select id="ipl-midi-in-select" style="width: 100%; padding: 6px 8px; background: #0f172a; color: #e2e8f0; border: 1px solid #334155; border-radius: 4px; font-size: 0.76rem;"></select>
              </div>
              <div>
                <label style="display: block; font-size: 0.72rem; color: #94a3b8; margin-bottom: 4px;">MIDI OUT Port (to MS2000 MIDI IN):</label>
                <select id="ipl-midi-out-select" style="width: 100%; padding: 6px 8px; background: #0f172a; color: #e2e8f0; border: 1px solid #334155; border-radius: 4px; font-size: 0.76rem;"></select>
              </div>
              <div>
                <label style="display: block; font-size: 0.72rem; color: #94a3b8; margin-bottom: 4px;" title="Matches KORG vup_ms2000.exe Data Transfer Rate setting. Use Fast/Slow if your USB-MIDI interface drops SysEx bytes.">Data Transfer Rate:</label>
                <select id="ipl-transfer-rate-select" style="width: 100%; padding: 6px 8px; background: #0f172a; color: #e2e8f0; border: 1px solid #334155; border-radius: 4px; font-size: 0.76rem;">
                  <option value="0">Very Fast (Default ~2 min)</option>
                  <option value="1">Fast (~2.5 min)</option>
                  <option value="2">Slow (~4 min)</option>
                  <option value="3">Very Slow (Safe ~6 min)</option>
                </select>
              </div>
              <div style="display: flex; gap: 6px;">
                <button id="ipl-refresh-ports-btn" class="diag-note-btn" style="padding: 6px 10px; font-size: 0.74rem;">Refresh Ports</button>
                <button id="ipl-check-unit-btn" class="diag-note-btn" style="padding: 6px 12px; font-size: 0.74rem; border-color: #38bdf8; color: #38bdf8;">Check Unit</button>
              </div>
            </div>
            <div style="display: flex; justify-content: space-between; align-items: center; margin-top: 8px; font-size: 0.72rem; color: #94a3b8;">
              <label style="display: flex; align-items: center; gap: 6px; cursor: pointer;">
                <input type="checkbox" id="ipl-autopoll-checkbox" checked style="accent-color: #38bdf8;">
                <span>Automatically detect when MS2000/R enters <code>IPL s.p.u</code> mode (polls every 2.5s)</span>
              </label>
              <span id="ipl-embedded-status" style="color: #38bdf8;">
                &#10003; Embedded <code>x811v107.sys</code> Ready (256 KB &bull; SHA-1 <code>945a7555...</code>)
              </span>
            </div>
          </div>

          <!-- 3. Firmware Update & 1 MB Flash Backup Actions -->
          <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 12px;">
            <!-- Card A: Official v1.07 Update -->
            <div class="diag-card" style="display: flex; flex-direction: column; justify-content: space-between;">
              <div>
                <div class="diag-card-title" style="color: #38bdf8;">3A. OFFICIAL SYSTEM OS v1.07 UPDATE</div>
                <div style="font-size: 0.75rem; color: #cbd5e1; line-height: 1.45; margin-bottom: 10px;">
                  Writes the embedded official KORG <strong>OS v1.07 (<code>x811v107.sys</code>)</strong> to the 256 KB System Flash area (32 blocks of 8 KB).
                  <br><span style="color: #94a3b8; font-size: 0.71rem;">&bull; Do NOT switch off power during transfer.<br>&bull; If interrupted, do NOT power off &mdash; click Update again.</span>
                </div>
              </div>
              <button id="ipl-install-v107-btn" class="card-action-btn" style="width: 100%; padding: 10px 14px; font-weight: 700; font-size: 0.8rem; background: linear-gradient(180deg, #0284c7 0%, #0369a1 100%); color: #fff; border: 1px solid #38bdf8; border-radius: 5px; cursor: pointer;">
                UPDATE HARDWARE TO OFFICIAL OS v1.07 (~2 MIN)
              </button>
            </div>

            <!-- Card B: Full 1 MB Flash Backup -->
            <div class="diag-card" style="display: flex; flex-direction: column; justify-content: space-between;">
              <div>
                <div class="diag-card-title" style="color: #a78bfa;">3B. FULL 1 MB FLASH BACKUP (flash.bin)</div>
                <div style="font-size: 0.74rem; color: #cbd5e1; line-height: 1.42; margin-bottom: 8px;">
                  Reads the complete 1 MB Flash (System + DWGS PCM + User Presets):
                  <br><span style="color: #94a3b8; font-size: 0.71rem;">1. Install the Flash-Dump IPL v1.07 patch, then power-cycle with <strong>WRITE + TYPE</strong>.<br>2. Click <strong>Read 1 MB Flash</strong> (~10 min). Use 3A anytime to restore unpatched v1.07.</span>
                </div>
              </div>
              <div style="display: flex; gap: 8px;">
                <button id="ipl-install-dumppatch-btn" class="card-action-btn" style="flex: 1; padding: 8px 10px; font-weight: 700; font-size: 0.74rem; background: rgba(139, 92, 246, 0.2); color: #ddd6fe; border: 1px solid rgba(167, 139, 250, 0.5); border-radius: 5px; cursor: pointer;">
                  1. Install Dump IPL Patch
                </button>
                <button id="ipl-read-flash-btn" class="card-action-btn" style="flex: 1; padding: 8px 10px; font-weight: 700; font-size: 0.74rem; background: linear-gradient(180deg, #7c3aed 0%, #5b21b6 100%); color: #fff; border: 1px solid #a78bfa; border-radius: 5px; cursor: pointer;">
                  2. Read 1 MB Flash (.bin)
                </button>
              </div>
            </div>
          </div>

          <!-- 4. Progress Bar, Post-Update Completion Banner & Diagnostic Log -->
          <div class="diag-card">
            <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 6px;">
              <span id="ipl-progress-stage" style="font-size: 0.76rem; font-weight: 600; color: #e2e8f0;">Ready &mdash; confirm hardware model and select MIDI ports.</span>
              <div style="display: flex; align-items: center; gap: 8px;">
                <span id="ipl-progress-pct" style="font-size: 0.76rem; font-weight: 700; color: #38bdf8;">0%</span>
                <button id="ipl-stop-btn" class="diag-panic-btn" style="padding: 3px 10px; font-size: 0.71rem;" disabled>STOP</button>
              </div>
            </div>
            <div style="width: 100%; height: 10px; background: #090d14; border: 1px solid #334155; border-radius: 5px; overflow: hidden; margin-bottom: 8px;">
              <div id="ipl-progress-bar" style="width: 0%; height: 100%; background: linear-gradient(90deg, #0284c7, #38bdf8); transition: width 0.15s ease;"></div>
            </div>

            <!-- Completion Banner (Hidden until update finishes) -->
            <div id="ipl-completion-banner" style="display: none; margin-bottom: 8px; padding: 9px 12px; border-radius: 5px; background: rgba(16, 185, 129, 0.16); border: 1px solid rgba(52, 211, 153, 0.5); color: #d1fae5; font-size: 0.76rem; line-height: 1.45;">
            </div>

            <textarea id="ipl-log-textarea" readonly style="width: 100%; height: 110px; background: #090d14; color: #94a3b8; border: 1px solid #1e293b; border-radius: 4px; padding: 8px; font-family: 'Courier New', monospace; font-size: 0.72rem; resize: none; line-height: 1.4;"></textarea>
          </div>
        </div>

        <div class="diag-footer" style="display: flex; justify-content: space-between; align-items: center;">
          <button id="ipl-copy-log-btn" class="diag-note-btn" style="padding: 6px 12px; font-size: 0.74rem;">Copy Diagnostic Log</button>
          <button id="ipl-close-btn" class="modal-close-btn">CLOSE FIRMWARE UTILITY</button>
        </div>
      </div>
    `;
  }

  bindDomEvents() {
    document.getElementById('ipl-close-top-btn')?.addEventListener('click', () => {
      if (!this.busy) this.close();
    });
    document.getElementById('ipl-close-btn')?.addEventListener('click', () => {
      if (!this.busy) this.close();
    });
    this.modal?.addEventListener('click', (e) => {
      if (e.target === this.modal && !this.busy) this.close();
    });

    const confirmCb = document.getElementById('ipl-confirm-checkbox');
    confirmCb?.addEventListener('change', (e) => {
      this.confirmed = e.target.checked;
      this.refreshControls();
      if (this.confirmed && this.selectedInputId && this.selectedOutputId) {
        this.triggerVersionCheck(true);
      }
    });

    const autoPollCb = document.getElementById('ipl-autopoll-checkbox');
    autoPollCb?.addEventListener('change', (e) => {
      this.autoPollEnabled = e.target.checked;
    });

    const inSel = document.getElementById('ipl-midi-in-select');
    const outSel = document.getElementById('ipl-midi-out-select');
    inSel?.addEventListener('change', (e) => {
      this.selectedInputId = e.target.value;
      this.unitDetected = false;
      this.syncSelectedPortsToBackend();
      this.refreshControls();
    });
    outSel?.addEventListener('change', (e) => {
      this.selectedOutputId = e.target.value;
      this.unitDetected = false;
      this.syncSelectedPortsToBackend();
      this.refreshControls();
    });

    const rateSel = document.getElementById('ipl-transfer-rate-select');
    rateSel?.addEventListener('change', (e) => {
      this.transferRate = Number(e.target.value) || 0;
    });

    document.getElementById('ipl-refresh-ports-btn')?.addEventListener('click', () => {
      this.requestPorts();
    });

    document.getElementById('ipl-check-unit-btn')?.addEventListener('click', () => {
      this.triggerVersionCheck(false);
    });

    document.getElementById('ipl-install-v107-btn')?.addEventListener('click', () => {
      this.startSystemInstall(false);
    });

    document.getElementById('ipl-install-dumppatch-btn')?.addEventListener('click', () => {
      this.startSystemInstall(true);
    });

    document.getElementById('ipl-read-flash-btn')?.addEventListener('click', () => {
      this.startFlashRead();
    });

    document.getElementById('ipl-stop-btn')?.addEventListener('click', () => {
      if (this.busy) {
        this.log('Requesting stop after current MIDI block...');
        bridge.send('ipl.cancel', {});
      }
    });

    document.getElementById('ipl-copy-log-btn')?.addEventListener('click', () => {
      const logEl = document.getElementById('ipl-log-textarea');
      if (logEl && navigator.clipboard) {
        navigator.clipboard.writeText(logEl.value);
      }
    });
  }

  bindBridgeEvents() {
    bridge.on('hardware.ports', (data) => {
      if (!data) return;
      this.inputs = Array.isArray(data.inputs) ? data.inputs : [];
      this.outputs = Array.isArray(data.outputs) ? data.outputs : [];
      this.populatePortSelects();
    });

    bridge.on('ipl.info', (data) => {
      if (!data) return;
      this.embeddedInfo = data;
      const statusEl = document.getElementById('ipl-embedded-status');
      if (statusEl) {
        if (data.embeddedValid) {
          statusEl.style.color = '#38bdf8';
          statusEl.innerHTML = `&#10003; Embedded <code>x811v107.sys</code> Verified (256 KB &bull; SHA-1 <code>${String(data.v107Sha1).slice(0, 10)}...</code>)`;
        } else {
          statusEl.style.color = '#f87171';
          statusEl.textContent = 'Embedded x811v107.sys checksum failed!';
        }
      }
      this.refreshControls();
    });

    bridge.on('ipl.version', (data) => {
      if (!data) return;
      if (data.ok) {
        const wasDetected = this.unitDetected;
        this.unitDetected = true;
        this.setLcd(data.lcdLine1 || 'IPL s.p.u [    ]', data.lcdLine2 || '................');
        this.setStateBadge('IPL MODE DETECTED', '#10b981', 'rgba(16, 185, 129, 0.2)');

        const sysEl = document.getElementById('ipl-ver-sys');
        const pcmEl = document.getElementById('ipl-ver-pcm');
        const usrEl = document.getElementById('ipl-ver-usr');
        if (sysEl) sysEl.textContent = `v${data.sysVersion}`;
        if (pcmEl) pcmEl.textContent = `v${data.pcmVersion}`;
        if (usrEl) usrEl.textContent = `v${data.usrVersion}`;

        if (!wasDetected || !data.silent) {
          this.log(`Hardware detected in IPL receive mode: System v${data.sysVersion}, PCM v${data.pcmVersion}, User v${data.usrVersion}.`);
          this.setStageText(`MS2000/R Connected in IPL Mode (System v${data.sysVersion}) — Ready for Update or Flash Backup.`);
        }
      } else {
        this.unitDetected = false;
        if (!data.silent) {
          this.setStateBadge('NO IPL RESPONSE', '#f59e0b', 'rgba(245, 158, 11, 0.2)');
          this.log(`Check Unit: ${data.error || 'No response'}`);
          this.setStageText(data.error || 'Unit not detected in IPL mode.');
        } else {
          this.setStateBadge('WAITING FOR IPL...', '#f59e0b', 'rgba(245, 158, 11, 0.16)');
        }
      }
      this.refreshControls();
    });

    bridge.on('ipl.progress', (data) => {
      if (!data) return;
      this.busy = true;
      const pct = Math.min(100, Math.max(0, Math.round(data.percent || 0)));
      const bar = document.getElementById('ipl-progress-bar');
      const pctEl = document.getElementById('ipl-progress-pct');
      if (bar) bar.style.width = `${pct}%`;
      if (pctEl) pctEl.textContent = `${pct}%`;
      if (data.stage) this.setStageText(data.stage);
      if (data.lcdLine1 || data.lcdLine2) {
        this.setLcd(data.lcdLine1 || 'IPL s.p.u [SYS ]', data.lcdLine2 || '................');
      }
      this.setStateBadge(data.operation === 'dump' ? 'READING FLASH...' : 'UPDATING FLASH...', '#38bdf8', 'rgba(56, 189, 248, 0.2)');
      this.refreshControls();
    });

    bridge.on('ipl.completed', (data) => {
      this.busy = false;
      if (!data) {
        this.refreshControls();
        return;
      }
      if (data.lcdLine1 || data.lcdLine2) {
        this.setLcd(data.lcdLine1 || 'IPL S.p.u [SYS ]', data.lcdLine2 || '>>>>............');
      }

      const banner = document.getElementById('ipl-completion-banner');
      if (data.ok) {
        const isPatch = data.operation === 'installDumpPatch';
        const secs = data.elapsedSeconds ? ` (${data.elapsedSeconds.toFixed(1)}s)` : '';
        this.setStateBadge('UPDATE COMPLETE', '#10b981', 'rgba(16, 185, 129, 0.25)');
        this.setStageText(isPatch
          ? `Flash-Dump IPL v1.07 installed${secs}! Power-cycle unit with WRITE + TYPE to read flash.`
          : `Official OS v1.07 Update Complete${secs}! Power-cycle your MS2000/R.`);
        this.log(isPatch
          ? `SUCCESS: Flash-Dump IPL v1.07 installed${secs}. Switch the unit OFF and ON again while holding WRITE + TYPE, then click "2. Read 1 MB Flash".`
          : `SUCCESS: Official KORG OS v1.07 installed${secs}. All 1-16 LEDs on the unit are now unlit. Switch the unit OFF and back ON for normal operation.`);

        if (banner) {
          banner.style.display = 'block';
          banner.style.background = 'rgba(16, 185, 129, 0.16)';
          banner.style.borderColor = 'rgba(52, 211, 153, 0.5)';
          banner.style.color = '#d1fae5';
          banner.innerHTML = isPatch
            ? `<strong>&#10003; Flash-Dump IPL v1.07 Installed!</strong><br>Now switch the MS2000/R <strong>OFF</strong>, hold <strong>WRITE + TYPE</strong> and switch it <strong>ON</strong> again, then click <strong>2. Read 1 MB Flash (.bin)</strong>.`
            : `<strong>&#10003; Official KORG OS v1.07 Update Completed Normally!</strong><br>1. Verify that LEDs 1&ndash;16 on your MS2000/R are now unlit.<br>2. Switch the MS2000/R power <strong>OFF</strong>, then back <strong>ON</strong> for normal operation.<br><em>Tip: To verify the OS version on the hardware LCD at any time, power on while holding <strong>MOD SEQ ON/OFF + Button 5</strong>.</em>`;
        }
      } else {
        this.setStateBadge('UPDATE ERROR', '#f87171', 'rgba(248, 113, 113, 0.25)');
        this.setStageText(`Error: ${data.error || 'Update failed'}`);
        this.log(`ERROR: ${data.error || 'Update failed'}`);
        if (banner) {
          banner.style.display = 'block';
          banner.style.background = 'rgba(127, 29, 29, 0.35)';
          banner.style.borderColor = 'rgba(248, 113, 113, 0.55)';
          banner.style.color = '#fecaca';
          banner.innerHTML = `<strong>&#9888; Update Interrupted:</strong> ${data.error || 'Transfer error'}<br><strong>IMPORTANT:</strong> Keep the MS2000/R power <strong>ON</strong> (the IPL is still running safely in RAM). Check your MIDI cables or select a slower <strong>Data Transfer Rate</strong>, then click Update again.`;
        }
      }
      this.refreshControls();
    });

    bridge.on('ipl.dumpCompleted', (data) => {
      this.busy = false;
      if (!data) {
        this.refreshControls();
        return;
      }
      if (data.lcdLine1 || data.lcdLine2) {
        this.setLcd(data.lcdLine1 || 'IPL s.p.u [    ]', data.lcdLine2 || '................');
      }

      const banner = document.getElementById('ipl-completion-banner');
      if (data.ok && data.dataBase64) {
        const secs = data.elapsedSeconds ? ` (${data.elapsedSeconds.toFixed(1)}s)` : '';
        this.setStateBadge('BACKUP COMPLETE', '#10b981', 'rgba(16, 185, 129, 0.25)');
        this.setStageText(`1 MB Flash Backup Complete${secs} — SHA-1: ${data.sha1}`);
        this.log(`SUCCESS: Read ${data.size} bytes${secs}. SHA-1: ${data.sha1}. System area verified: ${data.sysVerified ? 'YES' : 'NO'}.`);

        // Trigger browser download of flash.bin
        const binaryString = atob(data.dataBase64);
        const bytes = new Uint8Array(binaryString.length);
        for (let i = 0; i < binaryString.length; i++) {
          bytes[i] = binaryString.charCodeAt(i);
        }
        const blob = new Blob([bytes], { type: 'application/octet-stream' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = 'ms2000_flash_1MB.bin';
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);

        if (banner) {
          banner.style.display = 'block';
          banner.style.background = 'rgba(16, 185, 129, 0.16)';
          banner.style.borderColor = 'rgba(52, 211, 153, 0.5)';
          banner.style.color = '#d1fae5';
          banner.innerHTML = `<strong>&#10003; 1 MB Flash Backup Saved (<code>ms2000_flash_1MB.bin</code>)!</strong><br>SHA-1: <code>${data.sha1}</code> &bull; System Area Verified: <strong>${data.sysVerified ? 'OK' : 'Warning'}</strong>.<br>You can now click <strong>UPDATE HARDWARE TO OFFICIAL OS v1.07</strong> (3A) if you wish to restore the unpatched stock IPL.`;
        }
      } else {
        this.setStateBadge('BACKUP ERROR', '#f87171', 'rgba(248, 113, 113, 0.25)');
        this.setStageText(`Backup Failed: ${data.error || 'Read error'}`);
        this.log(`BACKUP FAILED: ${data.error || 'Read error'}`);
      }
      this.refreshControls();
    });
  }

  populatePortSelects() {
    const inSel = document.getElementById('ipl-midi-in-select');
    const outSel = document.getElementById('ipl-midi-out-select');
    if (!inSel || !outSel) return;

    const prevIn = this.selectedInputId;
    const prevOut = this.selectedOutputId;

    inSel.innerHTML = '<option value="">-- Select MIDI IN Port --</option>';
    this.inputs.forEach((p) => {
      const opt = document.createElement('option');
      opt.value = p.identifier;
      opt.textContent = p.name;
      inSel.appendChild(opt);
    });

    outSel.innerHTML = '<option value="">-- Select MIDI OUT Port --</option>';
    this.outputs.forEach((p) => {
      const opt = document.createElement('option');
      opt.value = p.identifier;
      opt.textContent = p.name;
      outSel.appendChild(opt);
    });

    if (prevIn && this.inputs.some((p) => p.identifier === prevIn)) {
      this.selectedInputId = prevIn;
    } else if (this.inputs.length > 0 && !this.selectedInputId) {
      this.selectedInputId = this.inputs[0].identifier;
    }

    if (prevOut && this.outputs.some((p) => p.identifier === prevOut)) {
      this.selectedOutputId = prevOut;
    } else if (this.outputs.length > 0 && !this.selectedOutputId) {
      this.selectedOutputId = this.outputs[0].identifier;
    }

    inSel.value = this.selectedInputId;
    outSel.value = this.selectedOutputId;
    this.syncSelectedPortsToBackend();
    this.refreshControls();
  }

  syncSelectedPortsToBackend() {
    if (this.selectedInputId || this.selectedOutputId) {
      bridge.send('hardware.selectPorts', {
        inputId: this.selectedInputId,
        outputId: this.selectedOutputId,
      });
    }
  }

  requestPorts() {
    bridge.send('hardware.listPorts', {});
    bridge.send('ipl.getInfo', {});
  }

  triggerVersionCheck(silent = false) {
    if (this.busy) return;
    if (!this.selectedInputId || !this.selectedOutputId) {
      if (!silent) this.log('Select both a MIDI IN and a MIDI OUT port first.');
      return;
    }
    this.syncSelectedPortsToBackend();
    if (!silent) {
      this.log('Sending IPL Version Request (F0 42 23 58 00 F7)...');
    }
    bridge.send('ipl.checkVersion', { silent });
  }

  startSystemInstall(installDumpPatch) {
    if (this.busy || !this.confirmed) return;
    if (!this.selectedInputId || !this.selectedOutputId) {
      this.log('Please select both MIDI IN and MIDI OUT ports before starting the update.');
      return;
    }

    const label = installDumpPatch
      ? 'Flash-Dump Patched IPL v1.07 (256 KB)'
      : 'Official KORG System OS v1.07 (x811v107.sys, 256 KB)';

    const banner = document.getElementById('ipl-completion-banner');
    if (banner) banner.style.display = 'none';

    this.busy = true;
    this.refreshControls();
    this.syncSelectedPortsToBackend();
    this.log(`Starting installation of ${label} (Transfer Rate: ${this.transferRate})...`);
    bridge.send('ipl.installSystem', {
      installDumpPatch,
      transferRate: this.transferRate,
    });
  }

  startFlashRead() {
    if (this.busy || !this.confirmed) return;
    if (!this.selectedInputId || !this.selectedOutputId) {
      this.log('Please select both MIDI IN and MIDI OUT ports before reading flash.');
      return;
    }

    const banner = document.getElementById('ipl-completion-banner');
    if (banner) banner.style.display = 'none';

    this.busy = true;
    this.refreshControls();
    this.syncSelectedPortsToBackend();
    this.log('Starting 1 MB Flash Backup (0x000000..0x0FFFFF, 8192 packets of 128 bytes)...');
    bridge.send('ipl.readFlashDump', {
      sizeBytes: 0x100000,
    });
  }

  setLcd(line1, line2) {
    const l1 = document.getElementById('ipl-lcd-line1');
    const l2 = document.getElementById('ipl-lcd-line2');
    if (l1 && line1) l1.textContent = String(line1).padEnd(16, ' ').slice(0, 16);
    if (l2 && line2) l2.textContent = String(line2).padEnd(16, '.').slice(0, 16);
  }

  setStateBadge(text, color, bg) {
    const badge = document.getElementById('ipl-state-badge');
    if (!badge) return;
    badge.textContent = text;
    badge.style.color = color;
    badge.style.background = bg;
  }

  setStageText(text) {
    const el = document.getElementById('ipl-progress-stage');
    if (el) el.textContent = text;
  }

  log(msg) {
    const area = document.getElementById('ipl-log-textarea');
    if (!area) return;
    const ts = new Date().toLocaleTimeString();
    area.value += `[${ts}] ${msg}\n`;
    area.scrollTop = area.scrollHeight;
  }

  refreshControls() {
    const canOperate = this.confirmed && !this.busy && Boolean(this.selectedInputId && this.selectedOutputId);
    const setDisabled = (id, dis) => {
      const el = document.getElementById(id);
      if (el) {
        el.disabled = dis;
        el.style.opacity = dis ? '0.45' : '1';
      }
    };

    setDisabled('ipl-midi-in-select', !this.confirmed || this.busy);
    setDisabled('ipl-midi-out-select', !this.confirmed || this.busy);
    setDisabled('ipl-transfer-rate-select', !this.confirmed || this.busy);
    setDisabled('ipl-refresh-ports-btn', !this.confirmed || this.busy);
    setDisabled('ipl-check-unit-btn', !canOperate);
    setDisabled('ipl-install-v107-btn', !canOperate || !this.embeddedInfo.embeddedValid);
    setDisabled('ipl-install-dumppatch-btn', !canOperate || !this.embeddedInfo.embeddedValid);
    setDisabled('ipl-read-flash-btn', !canOperate);
    setDisabled('ipl-stop-btn', !this.busy);
    setDisabled('ipl-close-btn', this.busy);
    setDisabled('ipl-close-top-btn', this.busy);
  }

  startAutoPoll() {
    this.stopAutoPoll();
    this.autoPollTimer = setInterval(() => {
      if (!this.modal || this.modal.classList.contains('hidden')) return;
      if (this.busy || !this.confirmed || !this.autoPollEnabled) return;
      if (!this.selectedInputId || !this.selectedOutputId) return;
      if (!this.unitDetected) {
        this.triggerVersionCheck(true);
      }
    }, 2500);
  }

  stopAutoPoll() {
    if (this.autoPollTimer) {
      clearInterval(this.autoPollTimer);
      this.autoPollTimer = null;
    }
  }

  open() {
    if (!this.modal) return;
    this.focusContract?.rememberTrigger();
    this.modal.style.display = 'flex';
    this.modal.classList.remove('hidden');
    this.focusContract?.releaseInert();
    this.focusContract?.focusFirst();

    this.requestPorts();
    this.refreshControls();
    this.startAutoPoll();
  }

  close() {
    if (!this.modal || this.busy) return;
    this.stopAutoPoll();
    this.focusContract?.restoreFocus();
    this.focusContract?.setInert();
    this.modal.style.display = 'none';
    this.modal.classList.add('hidden');
  }
}

export const firmwareUpdaterModal = new FirmwareUpdaterModalController();

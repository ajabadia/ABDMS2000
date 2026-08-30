export function createScopePanel(container, bridge) {
  container.innerHTML = `
    <div class="panel-card scope-panel">
      <div class="panel-header">
        <span class="panel-title">AUDIO TELEMETRY & OSCILLOSCOPE</span>
        <div class="scope-actions">
          <button id="scope-freeze-btn" class="menu-btn" style="padding: 2px 8px; font-size: 11px;">FREEZE</button>
        </div>
      </div>

      <div class="scope-body">
        <canvas id="scope-canvas" width="400" height="120" class="scope-canvas"></canvas>
        <div class="vu-meters">
          <div class="vu-bar"><div class="vu-fill" id="vu-l"></div></div>
          <div class="vu-bar"><div class="vu-fill" id="vu-r"></div></div>
        </div>
      </div>
    </div>
  `;

  const canvas = container.querySelector('#scope-canvas');
  const ctx = canvas.getContext('2d');
  const vuL = container.querySelector('#vu-l');
  const vuR = container.querySelector('#vu-r');
  const freezeBtn = container.querySelector('#scope-freeze-btn');

  let isFrozen = false;
  freezeBtn.addEventListener('click', () => {
    isFrozen = !isFrozen;
    freezeBtn.classList.toggle('active', isFrozen);
    freezeBtn.textContent = isFrozen ? 'RESUME' : 'FREEZE';
  });

  // Oscilloscope render loop
  const drawScope = (snapshot) => {
    if (isFrozen) return;

    ctx.fillStyle = 'rgba(10, 15, 20, 0.4)';
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    ctx.strokeStyle = 'var(--color-accent, #00c3ff)';
    ctx.lineWidth = 1.5;
    ctx.beginPath();

    const buffer = snapshot?.scopeBuffer || [];
    const len = buffer.length || 512;
    const sliceWidth = canvas.width / (len / 2);
    let x = 0;

    for (let i = 0; i < len / 2; ++i) {
      const v = (buffer[i] || 0.0) * 0.9;
      const y = (canvas.height / 2) - (v * (canvas.height / 2));

      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);

      x += sliceWidth;
    }
    ctx.stroke();

    // VU Meters
    if (vuL && vuR && snapshot) {
      const magL = Math.min(1.0, (snapshot.vuLeft || 0.0) * 1.5) * 100;
      const magR = Math.min(1.0, (snapshot.vuRight || 0.0) * 1.5) * 100;
      vuL.style.height = `${magL}%`;
      vuR.style.height = `${magR}%`;
    }
  };

  window.updateAudioSnapshot = drawScope;
}

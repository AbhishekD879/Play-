import './App.css';
import { ChangeEvent, DragEvent, useCallback, useEffect, useRef, useState } from 'react';
import { PlayModule } from './PlayModule';
import { useAppDispatch, useAppSelector, bootFile, init } from './Actions';

type PerformanceMode = 'auto' | 'full' | 'balanced' | 'fast';

interface PerformanceSnapshot {
  fps: number;
  speed: number;
  longFrames: number;
  jitCompiled: number;
  jitLive: number;
  jitCodeBytes: number;
  jitCompileMs: number;
  wasmMemoryBytes: number;
}

const EMPTY_SNAPSHOT: PerformanceSnapshot = {
  fps: 0,
  speed: 0,
  longFrames: 0,
  jitCompiled: 0,
  jitLive: 0,
  jitCodeBytes: 0,
  jitCompileMs: 0,
  wasmMemoryBytes: 0,
};

const MODES: ReadonlyArray<{ id: PerformanceMode; label: string; denominator: 1 | 2 | 3; detail: string }> = [
  { id: 'auto', label: 'Auto', denominator: 2, detail: 'Start balanced and move to Fast after sustained slowdown.' },
  { id: 'full', label: 'Full clock', denominator: 1, detail: 'Original EE clock for fast devices and maximum fidelity.' },
  { id: 'balanced', label: 'Balanced', denominator: 2, detail: 'Half EE clock for a practical speed and fidelity tradeoff.' },
  { id: 'fast', label: 'Fast', denominator: 3, detail: 'One-third EE clock for demanding games.' },
];

function readMetric(name: string): number {
  const method = PlayModule && PlayModule[name];
  if(typeof method !== 'function') return 0;
  const value = Number(method.call(PlayModule));
  return Number.isFinite(value) ? value : 0;
}

function formatBytes(value: number): string {
  if(!value) return '0 MB';
  return `${(value / 1024 / 1024).toFixed(value >= 100 * 1024 * 1024 ? 0 : 1)} MB`;
}

function usePerformanceSnapshot(enabled: boolean): PerformanceSnapshot {
  const [snapshot, setSnapshot] = useState(EMPTY_SNAPSHOT);
  const animationRef = useRef({ total: 0, long: 0, previous: 0 });

  useEffect(() => {
    if(!enabled) return;
    let animationFrame = 0;
    const inspectAnimation = (time: number) => {
      const monitor = animationRef.current;
      if(monitor.previous) {
        monitor.total += 1;
        if(time - monitor.previous > 25) monitor.long += 1;
      }
      monitor.previous = time;
      animationFrame = requestAnimationFrame(inspectAnimation);
    };
    animationFrame = requestAnimationFrame(inspectAnimation);
    const interval = window.setInterval(() => {
      const fps = readMetric('getFrames');
      if(PlayModule && typeof PlayModule.clearStats === 'function') PlayModule.clearStats();
      const monitor = animationRef.current;
      const longFrames = monitor.total ? (monitor.long / monitor.total) * 100 : 0;
      monitor.total = 0;
      monitor.long = 0;
      setSnapshot({
        fps,
        speed: Math.min(999, (fps / 59.94) * 100),
        longFrames,
        jitCompiled: readMetric('getJitBlocksCompiled'),
        jitLive: readMetric('getJitBlocksLive'),
        jitCodeBytes: readMetric('getJitCodeBytes'),
        jitCompileMs: readMetric('getJitCompileMs'),
        wasmMemoryBytes: PlayModule && PlayModule.HEAPU8 ? PlayModule.HEAPU8.byteLength : 0,
      });
    }, 1000);
    return () => {
      cancelAnimationFrame(animationFrame);
      window.clearInterval(interval);
    };
  }, [enabled]);

  return snapshot;
}

function App() {
  const state = useAppSelector((store) => store.play);
  const dispatch = useAppDispatch();
  const [fileName, setFileName] = useState('No game selected');
  const [dragging, setDragging] = useState(false);
  const [mode, setMode] = useState<PerformanceMode>('auto');
  const [effectiveClock, setEffectiveClock] = useState<'balanced' | 'fast'>('balanced');
  const [autoSamples, setAutoSamples] = useState<number[]>([]);
  const ready = state.value === 'initialized' || state.value === 'loaded';
  const isCrossOriginIsolated = typeof crossOriginIsolated !== 'undefined' && crossOriginIsolated;
  const snapshot = usePerformanceSnapshot(ready);

  useEffect(() => { void dispatch(init()); }, [dispatch]);

  const applyMode = useCallback((nextMode: PerformanceMode, automatic = false) => {
    if(!PlayModule) return;
    const resolvedMode = nextMode === 'auto' ? 'balanced' : nextMode;
    const denominator = MODES.find((candidate) => candidate.id === resolvedMode)!.denominator;
    if(typeof PlayModule.setEeFreqScale === 'function') PlayModule.setEeFreqScale(1, denominator);
    if(typeof PlayModule.setFrameLimit === 'function') PlayModule.setFrameLimit(true);
    if(!automatic) setMode(nextMode);
    setEffectiveClock(resolvedMode === 'fast' ? 'fast' : 'balanced');
    setAutoSamples([]);
  }, []);

  useEffect(() => {
    if(ready) applyMode('auto');
  }, [applyMode, ready]);

  useEffect(() => {
    if(mode !== 'auto' || snapshot.fps <= 0) return;
    const samples = autoSamples.concat(snapshot.speed).slice(-5);
    setAutoSamples(samples);
    if(samples.length < 5) return;
    const sorted = samples.slice().sort((left, right) => left - right);
    const median = sorted[Math.floor(sorted.length / 2)];
    if(effectiveClock === 'balanced' && median < 85) applyMode('fast', true);
  }, [applyMode, autoSamples, effectiveClock, mode, snapshot.fps, snapshot.speed]);

  const loadFiles = (files: FileList | null) => {
    const file = files && files[0];
    if(!file) return;
    setFileName(file.name);
    void dispatch(bootFile(file));
  };

  const handleChange = (event: ChangeEvent<HTMLInputElement>) => loadFiles(event.target.files);
  const handleDrop = (event: DragEvent<HTMLLabelElement>) => {
    event.preventDefault();
    setDragging(false);
    loadFiles(event.dataTransfer.files);
  };

  const exportSnapshot = () => {
    const data = { capturedAt: new Date().toISOString(), fileName, mode, effectiveClock, userAgent: navigator.userAgent, crossOriginIsolated: isCrossOriginIsolated, snapshot };
    const url = URL.createObjectURL(new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' }));
    const anchor = document.createElement('a');
    anchor.href = url;
    anchor.download = `play-web-perf-${Date.now()}.json`;
    anchor.click();
    URL.revokeObjectURL(url);
  };

  return (
    <main className="lab-shell">
      <header className="lab-header">
        <div><p className="eyebrow">Play! fork · isolated experiment</p><h1>Web performance lab</h1><p>Measure the emulator before an optimization is allowed anywhere near Game Hub production.</p></div>
        <div className="runtime-state" data-ready={ready || undefined}><i /><span><strong>{ready ? 'Runtime ready' : 'Initializing runtime'}</strong><small>{isCrossOriginIsolated ? 'Cross-origin isolated' : 'Isolation missing'}</small></span></div>
      </header>

      <section className="lab-workspace">
        <div className="game-stage">
          <div className="stage-heading"><span><strong>{fileName}</strong><small>{state.value}</small></span><span className="live-fps"><b>{snapshot.fps.toFixed(0)}</b> FPS</span></div>
          <canvas id="outputCanvas" width="640" height="480" tabIndex={-1} aria-label="PlayStation 2 emulator output" />
          <label className="drop-zone" data-dragging={dragging || undefined} onDragEnter={() => setDragging(true)} onDragLeave={() => setDragging(false)} onDragOver={(event) => event.preventDefault()} onDrop={handleDrop}>
            <input type="file" accept=".iso,.cso,.chd,.isz,.bin,.elf" onChange={handleChange} disabled={!ready} />
            <span><strong>{ready ? 'Choose a legal test game' : 'Waiting for WebAssembly'}</strong><small>ISO, CSO, CHD, ISZ, BIN or ELF · stays on this device</small></span>
          </label>
        </div>

        <aside className="telemetry-panel">
          <div className="panel-heading"><div><p className="eyebrow">Live telemetry</p><h2>Runtime health</h2></div><button onClick={exportSnapshot} disabled={!ready}>Export JSON</button></div>
          <div className="metric-grid">
            <Metric label="Frame rate" value={`${snapshot.fps.toFixed(0)} FPS`} />
            <Metric label="Emulation speed" value={`${snapshot.speed.toFixed(0)}%`} />
            <Metric label="Long frames" value={`${snapshot.longFrames.toFixed(1)}%`} />
            <Metric label="WASM heap" value={formatBytes(snapshot.wasmMemoryBytes)} />
            <Metric label="JIT blocks built" value={snapshot.jitCompiled.toLocaleString()} />
            <Metric label="JIT blocks live" value={snapshot.jitLive.toLocaleString()} />
            <Metric label="JIT code" value={formatBytes(snapshot.jitCodeBytes)} />
            <Metric label="JIT compile time" value={`${snapshot.jitCompileMs.toFixed(0)} ms`} />
          </div>

          <div className="mode-panel"><p className="eyebrow">EE clock experiment</p><div className="mode-list">{MODES.map((candidate) => <button key={candidate.id} data-active={mode === candidate.id || undefined} onClick={() => applyMode(candidate.id)}><span><strong>{candidate.label}</strong><small>{candidate.detail}</small></span><i>{candidate.id === 'auto' ? effectiveClock : `${candidate.denominator === 1 ? 100 : Math.round(100 / candidate.denominator)}%`}</i></button>)}</div></div>

          <div className="lab-actions"><button onClick={() => window.location.reload()}>Reconnect runtime</button><button onClick={() => PlayModule && typeof PlayModule.setFrameLimit === 'function' && PlayModule.setFrameLimit(false)} disabled={!ready}>Uncap benchmark</button></div>
          <p className="lab-note">This page is an instrumented test surface. It does not upload games, ship to Game Hub, or claim that an optimization is safe.</p>
        </aside>
      </section>
      <footer><span>Branch: codex/web-perf-lab</span><span>Version: {process.env.REACT_APP_VERSION || 'local'}</span></footer>
    </main>
  );
}

function Metric({ label, value }: { label: string; value: string }) {
  return <span className="metric"><small>{label}</small><strong>{value}</strong></span>;
}

export default App;

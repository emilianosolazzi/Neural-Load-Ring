import React, { useEffect, useMemo, useState } from 'react';

type Point = {
  t: number;
  coherence: number;      // 0-1 range
  microVar: number;       // 0-1 range
};

type CoherenceDashboardProps = {
  // Optional data source hook. If absent, a demo generator runs locally.
  fetchSample?: () => Promise<{ coherence: number; microVar: number; timestamp?: number } | null>;
  sampleIntervalMs?: number;
};

const MAX_POINTS = 180; // keep last 3 minutes at 1s cadence

function clamp01(v: number): number {
  return Math.min(1, Math.max(0, v));
}

function sparklinePath(points: Point[], accessor: (p: Point) => number): string {
  if (points.length === 0) return '';
  const xs = points.map((_, i) => i / Math.max(1, points.length - 1));
  const ys = points.map(accessor);
  const minY = Math.min(...ys, 0);
  const maxY = Math.max(...ys, 1);
  const range = Math.max(0.0001, maxY - minY);
  const normY = ys.map((y) => 1 - (y - minY) / range); // flip for svg
  return normY
    .map((y, i) => `${xs[i] * 100},${y * 40 + 5}`)
    .join(' ');
}

export const CoherenceDashboard: React.FC<CoherenceDashboardProps> = ({ fetchSample, sampleIntervalMs = 1000 }) => {
  const [data, setData] = useState<Point[]>([]);

  useEffect(() => {
    let mounted = true;
    const tick = async () => {
      const now = Date.now();
      let sample = null;
      if (fetchSample) {
        sample = await fetchSample();
      } else {
        // Demo generator: coherent baseline with slow drift and micro-variation
        const last = data[data.length - 1];
        const baseCoh = last ? last.coherence : 0.6;
        const baseVar = last ? last.microVar : 0.25;
        const driftCoh = baseCoh + (Math.random() - 0.5) * 0.05;
        const driftVar = baseVar + (Math.random() - 0.5) * 0.05;
        sample = {
          coherence: clamp01(driftCoh),
          microVar: clamp01(driftVar),
          timestamp: now,
        };
      }

      if (!mounted || !sample) return;
      const point: Point = {
        t: sample.timestamp ?? now,
        coherence: clamp01(sample.coherence),
        microVar: clamp01(sample.microVar),
      };
      setData((prev) => {
        const next = [...prev, point];
        if (next.length > MAX_POINTS) next.shift();
        return next;
      });
    };

    const id = setInterval(tick, sampleIntervalMs);
    return () => {
      mounted = false;
      clearInterval(id);
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [fetchSample, sampleIntervalMs]);

  const latest = data[data.length - 1];
  const prev = data[data.length - 6]; // ~5s back

  const coherenceDelta = useMemo(() => {
    if (!latest || !prev) return 0;
    return latest.coherence - prev.coherence;
  }, [latest, prev]);

  const microVarDelta = useMemo(() => {
    if (!latest || !prev) return 0;
    return latest.microVar - prev.microVar;
  }, [latest, prev]);

  const coherencePath = sparklinePath(data, (p) => p.coherence);
  const microVarPath = sparklinePath(data, (p) => p.microVar);

  return (
    <div style={{ display: 'grid', gap: 16, gridTemplateColumns: '1fr 1fr', maxWidth: 720 }}>
      <MetricCard
        title="Phase Coherence"
        value={latest ? (latest.coherence * 100).toFixed(1) + '%' : '—'}
        delta={coherenceDelta * 100}
        path={coherencePath}
        color="#4f46e5"
      />
      <MetricCard
        title="Micro-Variability"
        value={latest ? (latest.microVar * 100).toFixed(1) + '%' : '—'}
        delta={microVarDelta * 100}
        path={microVarPath}
        color="#16a34a"
      />
    </div>
  );
};

type MetricCardProps = {
  title: string;
  value: string;
  delta: number;
  path: string;
  color: string;
};

const MetricCard: React.FC<MetricCardProps> = ({ title, value, delta, path, color }) => {
  const deltaText = `${delta >= 0 ? '+' : ''}${delta.toFixed(1)}% over 5s`;
  return (
    <div style={{ border: '1px solid #e5e7eb', borderRadius: 12, padding: 16, background: '#fff' }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: 8 }}>
        <div style={{ fontWeight: 600 }}>{title}</div>
        <div style={{ color: '#6b7280', fontSize: 12 }}>{deltaText}</div>
      </div>
      <div style={{ fontSize: 28, fontWeight: 700, color: '#111827', marginBottom: 8 }}>{value}</div>
      <svg viewBox="0 0 100 50" role="presentation" aria-hidden="true">
        <polyline
          fill="none"
          stroke={color}
          strokeWidth={2}
          strokeLinejoin="round"
          strokeLinecap="round"
          points={path}
        />
      </svg>
    </div>
  );
};

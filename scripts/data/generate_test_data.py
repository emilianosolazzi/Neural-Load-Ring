#!/usr/bin/env python3
"""
Neural Load Ring Test Data Generator
Produces synthetic RR interval streams, PPG waveforms, and coherence datasets
for firmware, engine, and dashboard validation.

Usage:
    python generate_test_data.py --output-dir ./test_data --scenarios 5
"""

import argparse
import json
import numpy as np
from datetime import datetime, timedelta
from pathlib import Path
from typing import List, Dict, Tuple
import logging

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# Constants
SCENARIOS = {
    'rest': {
        'rr_base_ms': 850,
        'rr_variability_ms': 40,
        'coherence_proxy': 0.75,
        'duration_s': 300,
        'description': 'Resting state, high parasympathetic tone',
    },
    'work': {
        'rr_base_ms': 650,
        'rr_variability_ms': 25,
        'coherence_proxy': 0.45,
        'duration_s': 300,
        'description': 'Work stress, moderate sympathetic',
    },
    'exercise': {
        'rr_base_ms': 500,
        'rr_variability_ms': 15,
        'coherence_proxy': 0.30,
        'duration_s': 180,
        'description': 'Light exercise, high HR, reduced HRV',
    },
    'recovery': {
        'rr_base_ms': 750,
        'rr_variability_ms': 50,
        'coherence_proxy': 0.70,
        'duration_s': 300,
        'description': 'Post-exercise recovery, HRV restoration',
    },
    'sleep': {
        'rr_base_ms': 900,
        'rr_variability_ms': 60,
        'coherence_proxy': 0.85,
        'duration_s': 600,
        'description': 'Sleep-like state, very high HRV',
    },
}

PPG_SAMPLING_HZ = 100
RR_RESAMPLING_HZ = 4


class RRStreamGenerator:
    """Generates synthetic RR interval sequences with realistic HRV characteristics."""

    @staticmethod
    def generate(
        base_ms: float,
        variability_ms: float,
        coherence_proxy: float,
        duration_s: float,
        noise_factor: float = 0.05,
    ) -> List[float]:
        """
        Generate RR intervals with Kubios-inspired artifact and physiological realism.

        Args:
            base_ms: Base RR interval (rest ~850ms, exercise ~500ms)
            variability_ms: Beat-to-beat variation (RMSSD proxy)
            coherence_proxy: ANS coherence 0-1 (high=vagal, low=sympathetic)
            duration_s: Recording duration
            noise_factor: Sensor noise (0-1)

        Returns:
            List of RR intervals in milliseconds
        """
        num_samples = int(duration_s * (1000 / base_ms))
        rr_intervals = []

        # Initialize with a slow oscillation (respiratory modulation ~0.15 Hz)
        respiratory_phase = np.linspace(0, 2 * np.pi, num_samples)
        respiratory_modulation = 1 + 0.3 * np.sin(respiratory_phase)

        for i in range(num_samples):
            # Base + respiratory coupling + beat-to-beat noise
            respiratory_component = (base_ms * (respiratory_modulation[i] - 1)) * coherence_proxy
            beat_noise = np.random.normal(0, variability_ms)
            sensor_noise = np.random.normal(0, base_ms * noise_factor)

            rr = base_ms + respiratory_component + beat_noise + sensor_noise
            rr = np.clip(rr, base_ms * 0.7, base_ms * 1.5)  # Physiological bounds
            rr_intervals.append(float(rr))

        return rr_intervals


class PPGWaveformGenerator:
    """Generates synthetic PPG (photoplethysmography) waveforms at 100Hz."""

    @staticmethod
    def generate(rr_intervals: List[float], duration_s: float) -> List[float]:
        """
        Generate PPG waveform with cardiac oscillation at derived HR.

        Args:
            rr_intervals: RR intervals in milliseconds
            duration_s: Recording duration

        Returns:
            List of PPG samples (0-255 ADC-like values)
        """
        num_samples = int(duration_s * PPG_SAMPLING_HZ)
        ppg_samples = []

        # Mean HR from RR intervals
        mean_rr_ms = np.mean(rr_intervals)
        hr_bpm = 60000 / mean_rr_ms
        cardiac_freq_hz = hr_bpm / 60

        # DC baseline + AC modulation + noise
        for i in range(num_samples):
            t = i / PPG_SAMPLING_HZ

            # DC baseline (sensor DC offset)
            dc = 150

            # AC component (cardiac oscillation)
            ac = 50 * np.sin(2 * np.pi * cardiac_freq_hz * t)

            # Respiratory modulation (slower)
            respiratory = 20 * np.sin(2 * np.pi * 0.15 * t)

            # High-frequency noise (sensor noise)
            noise = np.random.normal(0, 5)

            ppg = dc + ac + respiratory + noise
            ppg = np.clip(ppg, 0, 255)
            ppg_samples.append(float(ppg))

        return ppg_samples


class HRVMetricsCalculator:
    """Computes time-domain and frequency-domain HRV metrics."""

    @staticmethod
    def calculate(rr_intervals: List[float]) -> Dict[str, float]:
        """
        Calculate HRV features: SDNN, RMSSD, HF/LF power proxies.

        Args:
            rr_intervals: RR intervals in milliseconds

        Returns:
            Dict with time and frequency domain metrics
        """
        rr_array = np.array(rr_intervals)
        diffs = np.diff(rr_array)

        # Time domain
        sdnn = float(np.std(rr_array))
        rmssd = float(np.sqrt(np.mean(diffs ** 2)))
        mean_hr = float(60000 / np.mean(rr_array))

        # Frequency domain (simplified Welch estimate)
        # HF (0.15-0.4 Hz): parasympathetic, high RMSSD correlation
        hf_power = float(rmssd ** 2 * 0.6)  # Vagal proxy
        # LF (0.04-0.15 Hz): mixed sympathetic and parasympathetic
        lf_power = float(sdnn ** 2 * 0.4)  # Longer-term variability

        return {
            'sdnn_ms': sdnn,
            'rmssd_ms': rmssd,
            'mean_hr_bpm': mean_hr,
            'hf_power': hf_power,
            'lf_power': lf_power,
            'lf_hf_ratio': lf_power / max(hf_power, 0.001),
        }


def generate_scenario(scenario_key: str, scenario_config: Dict) -> Dict:
    """Generate complete test dataset for a scenario."""
    logger.info(f'Generating {scenario_key}: {scenario_config["description"]}')

    rr_intervals = RRStreamGenerator.generate(
        base_ms=scenario_config['rr_base_ms'],
        variability_ms=scenario_config['rr_variability_ms'],
        coherence_proxy=scenario_config['coherence_proxy'],
        duration_s=scenario_config['duration_s'],
    )

    ppg_waveform = PPGWaveformGenerator.generate(
        rr_intervals=rr_intervals,
        duration_s=scenario_config['duration_s'],
    )

    hrv_metrics = HRVMetricsCalculator.calculate(rr_intervals)

    return {
        'scenario': scenario_key,
        'timestamp': datetime.now().isoformat(),
        'config': scenario_config,
        'rr_intervals_ms': rr_intervals,
        'ppg_samples': ppg_waveform,
        'hrv_metrics': hrv_metrics,
    }


def save_dataset(dataset: Dict, output_path: Path) -> None:
    """Save dataset to JSON with compact formatting."""
    with open(output_path, 'w') as f:
        json.dump(dataset, f, indent=2)
    logger.info(f'Saved: {output_path} ({output_path.stat().st_size / 1024:.1f} KB)')


def main():
    parser = argparse.ArgumentParser(description='Generate NLR test datasets')
    parser.add_argument('--output-dir', type=Path, default=Path('./test_data'),
                        help='Output directory')
    parser.add_argument('--scenarios', type=str, default='all',
                        help='Comma-separated scenario names or "all"')
    parser.add_argument('--seed', type=int, default=42, help='Random seed')
    args = parser.parse_args()

    np.random.seed(args.seed)
    args.output_dir.mkdir(parents=True, exist_ok=True)

    # Determine which scenarios to generate
    if args.scenarios.lower() == 'all':
        selected = SCENARIOS.keys()
    else:
        selected = [s.strip() for s in args.scenarios.split(',')]
        selected = [s for s in selected if s in SCENARIOS]

    # Generate datasets
    for scenario_key in selected:
        dataset = generate_scenario(scenario_key, SCENARIOS[scenario_key])
        output_file = args.output_dir / f'{scenario_key}_test_data.json'
        save_dataset(dataset, output_file)

    # Summary index
    index = {
        'generated_at': datetime.now().isoformat(),
        'scenarios': list(selected),
        'constants': {
            'ppg_sampling_hz': PPG_SAMPLING_HZ,
            'rr_resampling_hz': RR_RESAMPLING_HZ,
        }
    }
    with open(args.output_dir / 'index.json', 'w') as f:
        json.dump(index, f, indent=2)
    logger.info(f'Index: {args.output_dir / "index.json"}')


if __name__ == '__main__':
    main()
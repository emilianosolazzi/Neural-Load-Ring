import {
  parseRRNotification,
  parseCoherenceNotification,
  parseDeviceStateNotification,
  VibrationPattern,
} from '../src/services/bleService';

describe('bleService', () => {
  describe('parseRRNotification', () => {
    it('parses valid RR intervals from little-endian uint16 array', () => {
      // 800ms = 0x0320, 750ms = 0x02EE
      const data = new Uint8Array([0x20, 0x03, 0xEE, 0x02]);
      const result = parseRRNotification(data);
      expect(result).toEqual([800, 750]);
    });

    it('filters out physiologically invalid intervals', () => {
      // 100ms (too short), 800ms (valid), 3000ms (too long)
      const data = new Uint8Array([0x64, 0x00, 0x20, 0x03, 0xB8, 0x0B]);
      const result = parseRRNotification(data);
      expect(result).toEqual([800]); // Only valid interval
    });

    it('returns empty array for empty input', () => {
      const result = parseRRNotification(new Uint8Array([]));
      expect(result).toEqual([]);
    });

    it('handles odd-length data by ignoring trailing byte', () => {
      const data = new Uint8Array([0x20, 0x03, 0xFF]);
      const result = parseRRNotification(data);
      expect(result).toEqual([800]);
    });

    it('accepts boundary values 200ms and 2500ms', () => {
      // 200ms = 0x00C8, 2500ms = 0x09C4
      const data = new Uint8Array([0xC8, 0x00, 0xC4, 0x09]);
      const result = parseRRNotification(data);
      expect(result).toEqual([200, 2500]);
    });
  });

  describe('parseCoherenceNotification', () => {
    it('parses valid 12-byte coherence packet', () => {
      const data = new Uint8Array([
        35,        // stressLevel: 35
        72,        // coherencePct: 72
        90,        // confidencePct: 90
        55,        // variabilityLevel: 55
        0x20, 0x03, // meanRrMs: 800 (little-endian)
        0x23, 0x00, // rmssdMs: 35 (little-endian)
        0x8C, 0x00, // respiratoryRateCpm: 140 (14.0 cpm)
        0x00, 0x00, // padding
      ]);
      const result = parseCoherenceNotification(data);

      expect(result).toEqual({
        stressLevel: 35,
        coherencePct: 72,
        confidencePct: 90,
        variabilityLevel: 55,
        meanRrMs: 800,
        rmssdMs: 35,
        respiratoryRateCpm: 140,
      });
    });

    it('returns null for insufficient data', () => {
      const data = new Uint8Array([35, 72, 90, 55, 0x20, 0x03]); // Only 6 bytes
      const result = parseCoherenceNotification(data);
      expect(result).toBeNull();
    });

    it('returns null for empty data', () => {
      const result = parseCoherenceNotification(new Uint8Array([]));
      expect(result).toBeNull();
    });
  });

  describe('parseDeviceStateNotification', () => {
    it('parses valid 8-byte device state packet', () => {
      const data = new Uint8Array([
        85,        // batteryPct: 85%
        1,         // chargingState: charging
        2,         // connectionState: connected
        0x03,      // flags: streamingRR | streamingCoherence
        32,        // skinTempC: 32°C
        0,         // errorFlags: none
        0x3C, 0x00, // uptimeMin: 60 minutes
      ]);
      const result = parseDeviceStateNotification(data);

      expect(result).toEqual({
        batteryPct: 85,
        chargingState: 'charging',
        connectionState: 'connected',
        streamingRR: true,
        streamingCoherence: true,
        skinTempC: 32,
        errorFlags: 0,
        uptimeMin: 60,
      });
    });

    it('handles negative temperature (signed int8)', () => {
      const data = new Uint8Array([100, 0, 0, 0, 250, 0, 0x00, 0x00]); // 250 = -6°C
      const result = parseDeviceStateNotification(data);
      expect(result?.skinTempC).toBe(-6);
    });

    it('clamps invalid charging state index', () => {
      const data = new Uint8Array([100, 99, 0, 0, 32, 0, 0x00, 0x00]);
      const result = parseDeviceStateNotification(data);
      expect(result?.chargingState).toBe('full'); // Clamped to index 2
    });

    it('returns null for insufficient data', () => {
      const data = new Uint8Array([85, 1, 2, 0x03]); // Only 4 bytes
      const result = parseDeviceStateNotification(data);
      expect(result).toBeNull();
    });
  });

  describe('VibrationPattern enum', () => {
    it('has expected pattern values', () => {
      expect(VibrationPattern.OFF).toBe(0);
      expect(VibrationPattern.SINGLE).toBe(1);
      expect(VibrationPattern.DOUBLE).toBe(2);
      expect(VibrationPattern.TRIPLE).toBe(3);
      expect(VibrationPattern.HEARTBEAT).toBe(4);
      expect(VibrationPattern.BREATHING).toBe(5);
      expect(VibrationPattern.ALERT).toBe(6);
    });
  });
});

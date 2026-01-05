// Autonomic feedback pattern generators.
// Bridge between HRV/load analysis and hardware actuators.

export { generateThermalPattern, type ThermalPattern, type ThermalInput, type ThermalPrescription } from './thermalPatterns';
export { generateVibrationPattern, type VibrationPattern, type VibrationInput, type VibrationPrescription } from './vibrationPatterns';

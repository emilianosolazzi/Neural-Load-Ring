/**
 * Type declarations for React Native globals and modules without types
 */

/// <reference types="react-native" />

// React Native global
declare const __DEV__: boolean;

// Module augmentations for packages without proper types
declare module 'react-native-chart-kit' {
  export const LineChart: any;
  export const BarChart: any;
  export const PieChart: any;
  export const ProgressChart: any;
}

// Jest setup for React Native testing
// Don't spread react-native - mock only what we need

// Mock react-native-ble-plx
jest.mock('react-native-ble-plx', () => ({
  BleManager: jest.fn().mockImplementation(() => ({
    startDeviceScan: jest.fn(),
    stopDeviceScan: jest.fn(),
    connectToDevice: jest.fn().mockResolvedValue({
      id: 'TEST-DEVICE',
      discoverAllServicesAndCharacteristics: jest.fn().mockResolvedValue(undefined),
      monitorCharacteristicForService: jest.fn().mockReturnValue({ remove: jest.fn() }),
      readCharacteristicForService: jest.fn().mockResolvedValue({ value: null }),
      writeCharacteristicWithResponseForService: jest.fn().mockResolvedValue(undefined),
    }),
    cancelDeviceConnection: jest.fn().mockResolvedValue(undefined),
    onStateChange: jest.fn((callback, emitCurrentState) => {
      if (emitCurrentState) callback('PoweredOn');
      return { remove: jest.fn() };
    }),
    onDeviceDisconnected: jest.fn().mockReturnValue({ remove: jest.fn() }),
    state: jest.fn().mockResolvedValue('PoweredOn'),
    destroy: jest.fn(),
  })),
  State: {
    PoweredOn: 'PoweredOn',
    PoweredOff: 'PoweredOff',
    Unauthorized: 'Unauthorized',
    Unknown: 'Unknown',
  },
  Subscription: jest.fn(),
  Device: jest.fn(),
}));

// Mock react-native-chart-kit
jest.mock('react-native-chart-kit', () => ({
  LineChart: 'LineChart',
}));

// Mock react-native-svg
jest.mock('react-native-svg', () => ({
  Svg: 'Svg',
  Circle: 'Circle',
  Path: 'Path',
  G: 'G',
  Text: 'Text',
  Rect: 'Rect',
  Line: 'Line',
}));

// Mock @react-navigation
jest.mock('@react-navigation/native', () => ({
  useNavigation: () => ({
    navigate: jest.fn(),
    goBack: jest.fn(),
  }),
  useRoute: () => ({
    params: {},
  }),
  NavigationContainer: ({ children }) => children,
}));

jest.mock('@react-navigation/native-stack', () => ({
  createNativeStackNavigator: () => ({
    Navigator: ({ children }) => children,
    Screen: ({ children }) => children,
  }),
}));

// Mock AsyncStorage
jest.mock('@react-native-async-storage/async-storage', () => ({
  getItem: jest.fn().mockResolvedValue(null),
  setItem: jest.fn().mockResolvedValue(undefined),
  removeItem: jest.fn().mockResolvedValue(undefined),
  clear: jest.fn().mockResolvedValue(undefined),
}));

// Global test utilities
global.__DEV__ = true;

// Use fake timers to prevent timer leak warnings from animations/intervals
beforeEach(() => {
  jest.useFakeTimers();
});

afterEach(() => {
  jest.runOnlyPendingTimers();
  jest.useRealTimers();
});

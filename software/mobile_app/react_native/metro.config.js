const { getDefaultConfig, mergeConfig } = require('@react-native/metro-config');
const path = require('path');

const config = {
  watchFolders: [
    path.resolve(__dirname, '../../core_engine'),
  ],
  resolver: {
    extraNodeModules: {
      '@core': path.resolve(__dirname, '../../core_engine'),
    },
  },
};

module.exports = mergeConfig(getDefaultConfig(__dirname), config);

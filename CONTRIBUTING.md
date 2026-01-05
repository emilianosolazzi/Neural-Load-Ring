# Contributing to Neural Load Ring

Thank you for your interest in contributing to the Neural Load Ring project! This document provides guidelines and instructions for contributing.

## 📋 Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Workflow](#development-workflow)
- [Testing Requirements](#testing-requirements)
- [Pull Request Process](#pull-request-process)
- [Coding Standards](#coding-standards)
- [Component-Specific Guidelines](#component-specific-guidelines)

## Code of Conduct

Please be respectful and constructive in all interactions. We're building wellness technology—let's keep the development process healthy too.

## Getting Started

### Prerequisites

- **Node.js 18+** for TypeScript/React Native development
- **GCC** for firmware testing
- **Git** for version control

### Repository Structure

```
neural-load-ring/
├── software/
│   ├── core_engine/        # HRV analysis & haptic cue generation (TypeScript)
│   └── mobile_app/         # React Native mobile application
├── hardware/
│   ├── firmware/           # nRF52833 embedded code (C)
│   ├── schematics/         # KiCad PCB designs
│   └── mechanical/         # CAD files
├── docs/                   # Technical documentation
└── .github/                # CI/CD workflows
```

### Setting Up Development Environment

```bash
# Clone the repository
git clone https://github.com/your-org/neural-load-ring.git
cd neural-load-ring

# Core Engine
cd software/core_engine
npm install
npm test

# Mobile App
cd ../mobile_app/react_native
npm install
npm test

# Firmware Tests
cd ../../../hardware/firmware/tests
make
```

## Development Workflow

1. **Create a branch** from `develop`:
   ```bash
   git checkout develop
   git pull origin develop
   git checkout -b feature/your-feature-name
   ```

2. **Make your changes** following the coding standards below

3. **Run tests** before committing:
   ```bash
   # Core Engine
   cd software/core_engine && npm test
   
   # Mobile App
   cd software/mobile_app/react_native && npm test
   
   # Firmware
   cd hardware/firmware/tests && make
   ```

4. **Commit with meaningful messages**:
   ```bash
   git commit -m "feat(core): add respiratory detection algorithm"
   ```

5. **Push and create a Pull Request**

## Testing Requirements

### Core Engine
- All new algorithms must have unit tests
- Maintain >80% code coverage
- Test edge cases (empty arrays, boundary values)

### Mobile App
- Component tests for all UI components
- Service tests for BLE and data flow
- Mock external dependencies

### Firmware
- Unit tests for all signal processing functions
- Test with mock drivers for hardware abstraction

## Pull Request Process

1. Ensure all CI checks pass
2. Update documentation if needed
3. Request review from appropriate CODEOWNERS
4. Address review feedback
5. Squash commits if requested

### Commit Message Format

We use conventional commits:

```
type(scope): description

[optional body]

[optional footer]
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation
- `test`: Adding tests
- `refactor`: Code refactoring
- `ci`: CI/CD changes
- `deps`: Dependency updates

**Scopes:**
- `core`: Core engine
- `mobile`: Mobile app
- `firmware`: Firmware
- `hardware`: Hardware design
- `docs`: Documentation

## Coding Standards

### TypeScript (Core Engine & Mobile App)

```typescript
// Use explicit types
function processRRIntervals(intervals: number[]): AnalysisResult {
  // Implementation
}

// Use async/await over promises
async function fetchData(): Promise<Data> {
  const result = await api.get('/data');
  return result;
}

// Document public APIs
/**
 * Calculate heart rate variability metrics
 * @param rrIntervals - Array of RR intervals in milliseconds
 * @returns HRV analysis result
 */
export function analyzeHRV(rrIntervals: number[]): HRVResult {
  // Implementation
}
```

### C (Firmware)

```c
// Use descriptive names
void wellness_processor_handle_ppg_sample(int32_t sample);

// Document functions
/**
 * @brief Process incoming PPG sample
 * @param sample Raw ADC value from PPG sensor
 * @return true if peak detected
 */
bool detect_ppg_peak(int32_t sample);

// Use constants over magic numbers
#define PPG_SAMPLE_RATE_HZ 100
#define MIN_RR_INTERVAL_MS 300
```

## Component-Specific Guidelines

### Core Engine

The core engine implements validated HRV algorithms. When contributing:

- Reference peer-reviewed literature for algorithm changes
- Maintain backward compatibility with existing data formats
- Consider computational efficiency (runs on mobile devices)
- Update validation tests when changing algorithms

### Mobile App

- Follow React Native best practices
- Test on both iOS and Android (when possible)
- Handle BLE edge cases gracefully
- Consider offline scenarios

### Firmware

- Minimize memory allocations
- Consider power consumption
- Test with hardware-in-the-loop when possible
- Follow safety guidelines for haptic intensities

## Questions?

Open an issue with the `question` label or reach out to the maintainers.

---

Thank you for contributing to Neural Load Ring! 🎉

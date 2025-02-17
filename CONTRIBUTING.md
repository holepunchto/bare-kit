# CONTRIBUTING

## Dependencies

First, `bare-make` should be installed globally:

```bash
npm i -g bare-make
```

Then, install the dependencies:

```bash
npm i
```

## iOS

### Build

Generate the project:

```bash
bare-make generate --platform ios --arch arm64 --simulator
```

Build the project:

```bash
bare-make build
```

### Test

Start the simulator and run:

```bash
xcrun simctl spawn "<simulator_name>" ./build/test/apple/worklet.app/worklet
```

For instance for the iPhone 15:

```bash
xcrun simctl spawn "iPhone 15" ./build/test/apple/worklet.app/worklet
```

# Chronometro

Chronometro is a simple metronome.

- easy to divide a beat into eighth/triplet/sixteenth note
- use [Freesound - "Percussion clave like hit" by Sajmund](https://freesound.org/people/Sajmund/sounds/132417/) as sound

## Build

Build [JUCE](https://github.com/juce-framework/JUCE) app through [juce-cmake](https://github.com/remymuller/juce-cmake).

### macOS

```bash
cmake -D CMAKE_BUILD_TYPE:STRING=Debug -D JUCE_ROOT_DIR=<path-to-JUCE> -B <path-to-build> -G "Unix Makefiles"
cmake --build <path-to-build> --config Debug --target <target> -j <jobs>
```

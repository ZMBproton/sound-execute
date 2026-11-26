
# sound.execute();

A simple minimalist desktop audio player built using C++ and Qt 6. 

<img width="1782" height="1152" alt="sound-execute-screenshot" src="https://github.com/user-attachments/assets/671a4974-1bc0-472c-8e3b-9d18bdd5995c" />

## Features 

- Playlists
- Reorderable playback queue
- Change audio tempo and volume with sliders
- Supports Linux system-wide multimedia keys with MPRIS
- Shuffle and loop
- SQLite music library

## Requirements

- CMake 3.19+
- Qt 6.5+
- Qt Widgets, Multimedia, SQL, SVG, and D-Bus

## Build on Linux
Clone the repo, cd into the directory, and run these: 
```
cmake -S . -B build/Release -DCMAKE_BUILD_TYPE=Release
cmake --build build/Release -j
```

## Platform Support

Linux is the currently tested platform. Will include flatpak in the future


Windows support is planned!

## License

GNU General Public License v3.0.

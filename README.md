# midiripper

**midiripper** is a side project that captures screen pixels from a piano overlay (e.g., from online tutorials or visualizations) and converts the detected notes into MIDI data. The recorded MIDI data is saved as a standard `.mid` file.

This tool was created as a personal experiment to combine my interests in coding and music. It's not perfect, but it gets the job done for many use cases.

---

## Features

- **Screen Pixel Sampling**: Samples screen pixels from a piano overlay at ~60fps using WinGDI bitmap sampling.
- **MIDI File Generation**: Creates fully functional MIDI files using the [midifile library](https://github.com/craigsapp/midifile).
- **Simple Setup**: Made in C++ with WinGDI for minimal dependencies (and because I've never used other GUI frameworks).
- **Customizable Regions**: Define screen sample area, choose number of octaves to sample etc...

---

## How It Works

1. **Pixel Sampling**: midiripper identifies piano key activity by sampling screen regions at a frame rate of ~60fps. Each sampled point corresponds to a specific piano key.
2. **Note Detection**: Based on pixel color or brightness changes, it determines which keys are pressed.
3. **Note**: The "not pressed" piano colors are sampled when sampling starts. It will assume a deviation from those colors is a pressed note. This makes it compatible with any piano colors.
4. **MIDI Encoding**: Detected key events are converted into MIDI note events, and a MIDI file is generated.
5. **Save & Share**: The resulting MIDI file can be used in DAWs, virtual instruments, etc..

---

## Requirements

- **Platform**: Windows (uses WinGDI for screen sampling).
- **Language**: C++
- **Visual Studio 2019**: Compiled using VS2019
- **Libraries**: [midifile](https://github.com/craigsapp/midifile) (MIT Licence)

---
## Future-updates

-  I'm working on a viable sensitivity slider for a wider range of detection
-  Definitely need to make a recording indicator!! + prompt to save midi file
-  I'm considering a UI revamp with [Dear ImGui](https://github.com/ocornut/imgui) in future updates for better usability.
-  This is just a side project for fun, but contributions, suggestions, and feedback are welcome.
-  I don't really know how pull requests or git stuff works yet so you'll have to guide me on that if you'd like to contribute.

---

## Acknowledgments
- Thanks to [Craig Sapp](https://github.com/craigsapp) for the [midifile library](https://github.com/craigsapp/midifile). Saved me a lot of headache




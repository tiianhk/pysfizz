# pysfizz
Python bindings for [sfizz](https://github.com/sfztools/sfizz), a sample-based synthesizer for the [SFZ](https://sfzformat.com) virtual instrument format.

## Installation
### From PyPI
Prebuilt wheels are available for Python 3.9–3.13 on Linux, macOS, and Windows.
```bash
pip install pysfizz
```
### From source (development only)

**Requires:** Git, CMake 3.15+, C++17 compatible compiler, Python 3.9+ with development headers
```bash
git clone https://github.com/tiianhk/pysfizz.git
cd pysfizz
git submodule update --init --recursive
pip install .
```

## Example
```python
import pysfizz
import soundfile as sf

# load an instrument
synth = pysfizz.Synth(sample_rate=48000, block_size=1024)
synth.load_sfz_file("path/to/your/sfz/file.sfz")

# print the list of MIDI notes that have SFZ regions
print(synth.playable_keys)

# single note offline systhesis
pitch = 60       # MIDI note number, integer in [0, 127]
vel = 127        # MIDI velocity, integer in [0, 127]
note_dur = 1     # seconds, key pressed at t=0 and released after this duration
render_dur = 2   # seconds, total rendered duration
audio = synth.render_note(pitch, vel, note_dur, render_dur) # np.ndarray of shape (2, num_samples)
sf.write("output.wav", audio.T, synth.get_sample_rate())
```

## Resources
[SFZ instruments](https://sfzinstruments.github.io)

## License and dependencies

`pysfizz` is under the [BSD 2-Clause License](./LICENSE).

It includes the following dependencies as Git submodules under the `external/` directory:
- [sfizz](https://github.com/sfztools/sfizz) — BSD 2-Clause License  
- [nanobind](https://github.com/wjakob/nanobind) — BSD 3-Clause License

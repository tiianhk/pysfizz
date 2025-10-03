## Build from Source

### Prerequisites
- Python 3.9 or higher with development headers
- CMake 3.15 or higher
- C++17 compatible compiler

### Build Steps
1. Clone the repository & initialize submodules:
```bash
git clone https://github.com/tiianhk/pysfizz.git
cd pysfizz
git submodule update --init --recursive
```
2. Build the Python module:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```
3.  Test the Python module:
```bash
python -c "import pysfizz; print('pysfizz imported successfully!')"
```

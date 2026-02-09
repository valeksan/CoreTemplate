# Contributing to CoreTaskManager

Thank you for your interest in contributing to CoreTaskManager! 🙏  
We strive to make the contribution process as smooth and transparent as possible.

## How to Contribute

We welcome contributions in many forms:

*   **🐛 Reporting Bugs** — Please open an issue with clear steps to reproduce.
*   **✨ Suggesting Enhancements** — Share your ideas via an issue.
*   **💻 Submitting Code Changes** — Fork, code, and open a pull request (PR).

## Development Setup

### Prerequisites
*   C++ compiler (GCC, Clang, MSVC) supporting **C++17+**
*   [CMake](https://cmake.org/) ≥ 3.16
*   [Qt 6.x](https://www.qt.io/) (preferably 6.10.2)

### Building the Example
The project uses CMake. You can build it in two ways:

#### 1. Command Line
```bash
cd example
mkdir build && cd build
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build .

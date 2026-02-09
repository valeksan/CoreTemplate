<!-- markdownlint-disable MD033 -->

# Contributing to CoreTaskManager

<div align="center">

[![Contributors][contributors-shield]][contributors-url]
[![Forks][forks-shield]][forks-url]
[![Stargazers][stars-shield]][stars-url]
[![Issues][issues-shield]][issues-url]

</div>

<div align="center">
  <sub>Thank you for considering contributing to CoreTemplate!</sub>
</div>

<br />

We love your input! 🎉 We want to make contributing to this project as easy and transparent as possible, whether it's:

*   Reporting a bug 🐛
*   Discussing the current state of the code 🗣️
*   Submitting a fix 🩹
*   Proposing new features 🚀
*   Becoming a maintainer 👑

## Table of Contents

*   [How to Contribute](#how-to-contribute)
*   [Development Setup](#development-setup)
*   [Pull Request Guidelines](#pull-request-guidelines)
*   [Code Style](#code-style)
*   [Questions?](#questions)

## How to Contribute

There are many ways you can contribute! Here are some pointers to get you started:

*   **Reporting Bugs:** Found a bug? 😰 Please [open an issue][issues-url] and describe it thoroughly. Include steps to reproduce, expected behavior, and your environment (OS, Qt version, compiler, etc.).
*   **Suggesting Enhancements:** Got an idea for a new feature or an improvement? 🚀 We'd love to hear it! [Open an issue][issues-url] and let us know your thoughts.
*   **Submitting Code Changes:** Ready to contribute code? 🧑‍💻 Great! Fork the repository, make your changes, and submit a pull request (PR). Please ensure your code adheres to the [project's style](#code-style) and includes tests if applicable.

## Development Setup

Before you start contributing, ensure your development environment is ready.

### Prerequisites

*   A C++ compiler supporting **C++17** or later (e.g., GCC, Clang, MSVC).
*   [**CMake**](https://cmake.org/) (version **3.16** or higher).
*   [**Qt 6.x**](https://www.qt.io/) (preferably **6.10.2** or compatible).
*   A compatible build system (e.g., Ninja, GNU Make, MSBuild) or an IDE like Qt Creator.

### Cloning the Repository

```bash
git clone https://github.com/valeksan/CoreTemplate.git # Note: The repo name might differ slightly
cd CoreTemplate # Or the actual folder name after cloning
```

### Building the Project example

This project uses CMake. Here are two common ways to build the example application located in the example/ directory:

#### Option 1: Command Line Interface (CLI)
1. Navigate to the example directory:
   ```bash
   cd example
   ```
2. Create a build directory:
   ```bash
   mkdir build && cd build
   ```
3. Configure the project using CMake. Choose a generator that matches your system (e.g., `"Ninja"`, `"Unix Makefiles"`, `"Visual Studio 17 2022"`).
   ```bash
   cmake .. -G "<Your_Generator>" -DCMAKE_BUILD_TYPE=Release
   ```
   *(Replace <Your_Generator> with your choice)*
4. Build the project:
   ```bash
   cmake --build .
   ```

#### Option 2: Using Qt Creator
1. Launch Qt Creator.
2. Open the `example/CMakeLists.txt` file directly from the project root.
3. Qt Creator will automatically detect the CMake project. Select a suitable Kit (compiler/Qt version).
4. Use the Build menu or the hammer icon to build the project.

## Pull Request Guidelines

Your PRs are essential! 🙌 Please follow these guidelines to keep things smooth:
- **Discuss First (if significant):** For large or significant changes, consider opening an issue first to discuss the proposed approach.
- **One PR, One Topic:** Keep pull requests focused on a single subject. If you have multiple unrelated changes, submit them separately.
- **Follow the Style:** Ensure your code aligns with the project's coding style.
- **Update Documentation:** If you add or modify features, update the corresponding documentation (README, comments, this file, etc.).
- **Include Tests:** Add tests for new functionality or bug fixes if possible.
- **Test Locally:** Verify that your changes build correctly and pass any existing tests on your local machine before submitting.
- **Clear Descriptions:** Provide a clear and descriptive title and description for your pull request explaining the changes made.

## Code Style

Consistent code style keeps the project readable and maintainable! 🧼
- **Follow Best Practices:** Adhere to common C++ and Qt best practices (e.g., RAII, smart pointers, Qt's signal-slot mechanism).
- **Naming Conventions:**
  * Use `camelCase` for functions and variables (e.g., `registerTask`, `m_taskHash`).
  * Use `PascalCase` for classes and structs (e.g., `Core`, `TaskInfo`).
  * Use `UPPER_SNAKE_CASE` for constants defined via `#define` (e.g., `DEFAULT_STOP_TIMEOUT`).
- **Comments:** Add comments to clarify complex logic or non-obvious design decisions. Document public APIs clearly.
- **Header File (`core.h`):** Ensure it remains self-contained and well-documented. Avoid unnecessary includes.

## Questions?

Still have questions or need clarification? 😕 Don't hesitate to open an issue or start a discussion in the repository. We're here to help!



*Thank you again for your interest in contributing! 🙏 Your efforts help make CoreTaskManager better for everyone.*

<!-- MARKDOWN LINKS -->
[contributors-shield]: https://img.shields.io/github/contributors/valeksan/CoreTemplate.svg?style=for-the-badge
[contributors-url]: https://github.com/valeksan/CoreTemplate/graphs/contributors
[forks-shield]: https://img.shields.io/github/forks/valeksan/CoreTemplate.svg?style=for-the-badge
[forks-url]: https://github.com/valeksan/CoreTemplate/network/members
[stars-shield]: https://img.shields.io/github/stars/valeksan/CoreTemplate.svg?style=for-the-badge
[stars-url]: https://github.com/valeksan/CoreTemplate/stargazers
[issues-shield]: https://img.shields.io/github/issues/valeksan/CoreTemplate.svg?style=for-the-badge
[issues-url]: https://github.com/valeksan/CoreTemplate/issues

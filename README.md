# Spelunky (in C)

<img src="https://github.com/user-attachments/assets/83653198-c3ca-43d6-9806-126987b8319e" alt="Game Screenshot" style="max-width: 100%; height: auto;" />

---

## About

This project was made in 42 hours for UQCS 2025 Hackathon. It is an attempted clone of the classic game "Spelunky" by Derek Yu but in C. I strictly prohibited myself from using AI-generated code in any way, including no auto-completions.

Given the very strict time constraints and the fact that I had no plans to further develop this beyond the event, the code here is absolutely catastrophic spaghetti and wasn't thought through well at all. I apologise for any headaches you develop while reading it.

> **Note:** This project uses [Zeta Framework](https://github.com/harrisonkwhite/zeta_framework), a simple framework I made for developing 2D games using OpenGL.

---

## Building

Building and running this project has been tested on Linux and Windows.

Clone the repository by running `git clone --recursive https://github.com/harrisonkwhite/spelunky_in_c.git`.

> **Note:** If the repository was cloned non-recursively before, just run `git submodule update --init --recursive` to clone the necessary submodules.

Then go into the repository root and build with CMake:

```
mkdir build
cd build
cmake ..
```

For Linux, there are a number of dependencies you might need to manually install. CMake will report if any are missing.

You can also use the "run.sh" shell script if on a platform which supports it.

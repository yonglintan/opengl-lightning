# Real-Time Procedural Lightning Generator

A highly configurable, real time 3D lightning effect generator built using OpenGL. Parameters like color, intensity, erraticity, and 3D anchor points can be dynamically modified to create lightning with the desired visual effect.

## Getting Started

### **Requirements**

This project depends on:

- cmake
- vcpkg

### **Steps**

1.  Install cmake and vcpkg
    - Verify whether or not cmake is installed by running:
    ```sh
    cmake --version
    ```
    - If cmake is not installed, please install it. cmake is available to download through the [official website](https://cmake.org/). MacOS users can also install through homebrew with `brew install cmake`. Linux users can likely install cmake through their package managers.
    - Verify whether or not vcpkg is installed by running:
    ```sh
    vcpkg version
    ```
    - if vcpkg is not installed, install it by referring to the first steps of the [getting started instructions](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started?pivots=shell-powershell). Ensure that the `VCPKG_ROOT` environment variable is configured according to the instructions.
2.  Clone the repository
    - Open your terminal to your desired location, then run:
    ```sh
    git clone https://github.com/yonglintan/opengl-lightning.git
    ```
3.  Configure the build with cmake
    - Navigate into the project root folder:
    ```sh
    cd opengl-lightning
    ```
    - Configure build using cmake preset
      - To use the Visual Studio 2022 build system (must be on Windows and have Visual Studio 2022 installed):
      ```sh
      cmake --preset=vcpkg-vs
      ```
      - Or, on MacOS/Linux, to use makefiles:
      ```sh
      cmake --preset=vcpkg-make
      ```
      - Else, specify the appropriate [generator](https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html#cmake-generators) using the `-G` flag:
      ```sh
      cmake --preset=vcpkg-make -G "Your Generator"
      ```
4.  Build and run the project

    - From the project root folder, run:

    ```sh
    cmake --build build
    ```

    - Run the built executable:
      - Location of the executable may vary. The `cmake --build build` command previously run should have indicated the location of the executable.

5.  For Zac
    - Check if your vcpkg exists and works with:
    ```sh
    ~/vcpkg/vcpkg --version
    ```
    - If not found, use:
    ```sh
    export PATH=$HOME/vcpkg:$PATH
    ```
    - If issues are still found, do:
    ```sh
    rm -rf build
    mkdir build && cd build
    ```
    and also,
    ```sh
    cmake .. -G "Unix Makefiles" \
    -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DCMAKE_C_COMPILER=$(which clang) \
    -DCMAKE_CXX_COMPILER=$(which clang++)
    ```
    - Stuff you like to use and always forget
    ```sh
    cmake --build build
    build/GraphicsProj
    ```

### 1) Install prerequire-sites

    sudo apt install \
        build-essential \
        cmake \
        ninja-build \
        git \
        libwayland-dev \
        libx11-dev \
        libxext-dev \
        libxrandr-dev \
        libxcursor-dev \
        libxi-dev \
        libxinerama-dev \
        libxfixes-dev \
        libxkbcommon-dev \
        libgl1-mesa-dev

### 2) Download sources
  
    git clone https://github.com/libsdl-org/SDL.git

### 3) Select branch

    cd SDK
    git branch -vva
    git checkout release-3.4.x

### 4) Compile

    cd ..
    cmake -S SDL -B cmake-build-release -G Ninja
    cmake --build cmake-build-release

### 5) Install

    sudo cmake --install cmake-build-release
    sudo ldconfig

### 6) Check installed

    pkg-config --modversion sdl3

### 7) Uninstall

    sudo xargs rm < cmake-build-release/install_manifest.txt
    sudo ldconfig

# Build How-to

This is a quick start guide for building nijigenerate and nijiexpose. For users and developers,
you can follow the instructions below to build on your Linux system.

Also See [build-and-release.yml in nijigenerate](https://github.com/nijigenerate/nijigenerate/blob/main/.github/workflows/build-and-release.yml) 
and [build-and-release.yml in nijiexpose](https://github.com/nijigenerate/nijiexpose/blob/main/.github/workflows/build-and-release.yml)

## Install Dependencies
### Ubuntu
```bash
sudo apt update
sudo apt install build-essential dub cmake git libsdl2-dev libfreetype6-dev
```

For Ubuntu 24.04, install the compiler
```
sudo apt install ldc 
```

For Ubuntu 22.04 uses an older version of `ldc2`, you must install `dmd`
```
sudo apt install curl
curl -fsS https://dlang.org/install.sh | bash -s dmd
```

Make sure you activate `dmd` environment before compiling, The installation script will provide a version hint when it is complete.
```
source ~/dlang/dmd-2.109.1/activate
```

### Archlinux
```bash
sudo pacman -Syu
sudo pacman -S ldc base-devel dub cmake git sdl2 freetype2
```

### Fedora
```bash
sudo dnf -y install \
    git \
    dub \
    ldc \
    cmake \
    gcc-c++ \
    SDL2-devel \
    freetype-devel \
    dbus-devel
```

### macos
```bash
brew update
brew install create-dmg gettext

# for mcp server
brew install openssl@3 

# LDC v1.42 has a dynamic-cast bug; use v1.41.0 instead [ldc#5079](https://github.com/ldc-developers/ldc/issues/5079)
curl -fsS https://dlang.org/install.sh | bash -s install ldc-1.41.0
source ~/dlang/ldc-1.41.0/activate
```
For other dependencies, refer to Linux and use brew commands instead.

For `x86_64` builds you need to install the `x86_64` version of brew, then install openssl@3

### macOS troubleshooting

For arm64-only builds, in `i2d-imgui/deps/CMakeLists.txt` set:
```
set(CMAKE_OSX_ARCHITECTURES "arm64")
```
instead of `"arm64;x86_64"`.

## Build nijilive Project
First, we need to clone the four projects under nijigenerate and add them to `dub add-local`.
```bash
git clone https://github.com/nijigenerate/nijigenerate
dub add-local ./nijigenerate 0.0.1
git clone https://github.com/nijigenerate/nijilive
dub add-local ./nijilive 0.0.1
git clone https://github.com/nijigenerate/nijiui
dub add-local ./nijiui 0.0.1 
git clone https://github.com/nijigenerate/nijiexpose
dub add-local ./nijiexpose 0.0.1
```

nijigenerate uses an older version of `i2d-imgui`; pin the same commit as CI:
```bash
git clone --recurse-submodules https://github.com/Inochi2D/i2d-imgui
cd i2d-imgui
git checkout c6a78f4a7510fd31a86998b7ceedfc2916ecfae0
git submodule update --recursive
cd ..
dub add-local i2d-imgui 0.8.0
```

On Ubuntu 24.04 LTS, a link error may occur. To fix the libz issue, use `vim ./nijigenerate/dub.sdl` to find this line:
```
lflags "-rpath=$$ORIGIN" platform="linux"
```
add `"-lz"` flag
```
lflags "-rpath=$$ORIGIN" "-lz" platform="linux"
```

To build nijigenerate, if you use dmd, please replace `--compiler=ldc2` with `--compiler=dmd`
```
cd nijigenerate
dub build --config=meta
dub build --compiler=ldc2 --build=release --config=linux-full
./out/nijigenerate
```
build nijiexpose
```
cd nijiexpose
dub build --config=meta
dub build --compiler=ldc2 --build=release --config=linux-full
./out/nijiexpose
```

## Packing up a dmg file
```
./gentl.sh
# macos arm64
dub build --compiler=ldc2 --build=release --config=osx-full --arch=arm64-apple-macos
# macos x86_64
# dub build --compiler=ldc2 --build=release --config=osx-full --arch=x86_64-apple-macos
./build-aux/osx/osxbundle.sh
./build-aux/osx/gendmg.sh
```

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

dub package `i2d-imgui` will use the `Master` branch by default when building SDL2,
but the upstream branch has been changed to `main`. We need to fix it with the following instructions.
```bash
git clone --recurse-submodules https://github.com/Inochi2D/i2d-imgui
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


# nijigenerate
![Image of nijigenerate](https://user-images.githubusercontent.com/7032834/194462402-74c4a3e0-50ca-4b50-8e8d-164d97371f5a.png)
_Ada model by [ku-ini](https://twitter.com/duckmastah)_

----------------

[![Support me on Patreon](https://img.shields.io/endpoint.svg?url=https%3A%2F%2Fshieldsio-patreon.vercel.app%2Fapi%3Fusername%3Dclipsey%26type%3Dpatrons&style=for-the-badge)](https://patreon.com/clipsey)
[![Join the Discord](https://img.shields.io/discord/855173611409506334?label=Community&logo=discord&logoColor=FFFFFF&style=for-the-badge)](https://discord.com/invite/abnxwN6r9v)

nijigenerate is an open source editor for the [nijilife puppet format](https://github.com/nijigenerate/nijilife).  This application allows you to rig models for use in games or for other real-time applications such as [VTubing](https://en.wikipedia.org/wiki/VTuber). Animation is achieved by morphing, transforming and in other ways distorting layered 2D textures in real-time. These distortions can trick the end user in to perciving 3D depth in the 2D art.

If you are a VTuber wanting to use nijilife we highly recommend checking out [Inochi Session](https://github.com/nijilife/inochi-session) as well.

&nbsp;

## Downloads

### Stable Builds

&nbsp;&nbsp;&nbsp;&nbsp;
[![Buy on itch.io](https://img.shields.io/github/v/release/nijigenerate/nijigenerate?color=%23fa5c5c&label=itch.io&logo=itch.io&style=for-the-badge)](https://lunafoxgirlvt.itch.io/nijigenerate)

### Experimental Builds

&nbsp;&nbsp;&nbsp;&nbsp;
[![Nightly Builds](https://img.shields.io/github/actions/workflow/status/nijigenerate/nijigenerate/nightly.yml?label=Nightly&style=for-the-badge)](https://github.com/nijigenerate/nijigenerate/releases/tag/nightly)  

&nbsp;

## For package maintainers
We do not officially support packages that we don't officially build ourselves, we ask that you build using the barebones configurations, as the branding assets are copyright the nijilife Project.  
You may request permission to use our branding assets in your package by submitting an issue.

Barebones builds are more or less equivalent to official builds with the exception that branding is removed,  
and that we don't accept support tickets unless a problem can be replicated on an official build.

Links in `source/nijigenerate/config.d` should be updated to point to your package's issues list, as we do not accept issues from non-official builds.

&nbsp;

## Building
It's occasionally the case that our dependencies are out of sync with dub, so it's somewhat recommended if you're building from source to clone the tip of `main` and `dub add-local . "<version matching nijigenerate dep>"` any of our forked dependencies (i18n-d, psd-d, bindbc-imgui, facetrack-d, inmath, nijilife). This will generally keep you up to date with what we're doing, and it's how the primary contributors work. Ideally we'd have a script to help set this up, but currently we do it manually, PRs welcome :)

Because our project has dependencies on C++ through bindbc-imgui, and because there's no common way to get imgui binaries across platforms, we require a C++ toolchain as well as a few extra dependencies installed. These will be listed in their respective platform sections below.  
Currently you **have** to _recursively_ clone bindbc-imgui from git and set its version to `0.7.0`, otherwise the build will fail.

Once the below dependencies are met, building and running nijigenerate should be as simple as calling `dub` within this repo.

### Windows
#### Dependencies
- Visual Studio 2022 (With "Desktop development with C++" workflow installed)
  - In theory, "Build Tools for Visual Studio 2022" should also work, but is untested.
- CMake (Currently 3.16 or higher is needed.)
- Dlang, either dmd or ldc (ldc recommended)

### Linux
#### Dependencies
- The equivalent of build-essential on Ubuntu, on centos 7, this was `sudo yum groupinstall 'Development Tools'`, this should get you a working C++ toolchain.
- Dlang, either dmd or ldc (ldc recommended)
- CMake (Currently 3.16 or higher is needed.)
- SDL2 (developer package)
- Freetype (developer package)
- appimagetool (for building an AppImage)
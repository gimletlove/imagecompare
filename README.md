# Image Compare — Linux Image Comparison & Visual Diff Tool

[![GitHub Release](https://img.shields.io/github/v/release/gimletlove/imagecompare?label=release)](https://github.com/gimletlove/imagecompare/releases)
[![Flathub](https://img.shields.io/flathub/v/io.github.gimletlove.imagecompare?label=Flathub)](https://flathub.org/apps/details/io.github.gimletlove.imagecompare)
[![AUR](https://img.shields.io/aur/version/imagecompare-bin?label=AUR)](https://aur.archlinux.org/packages/imagecompare-bin)
[![License: GPL-3.0](https://img.shields.io/github/license/gimletlove/imagecompare)](./LICENSE.txt)

**Image Compare** is a Linux desktop app for comparing two or more images side by side, in a stacked view, or with perceptual heatmap differences. It supports synchronized zoom and pan, grid layouts, multiple image formats, faithful color-profile rendering, and fast image loading with Qt6 and libvips.

Use it to compare screenshots, rendered images, exports, design revisions, before/after edits, and subtle visual differences between image versions.

## Screenshots

![Image Compare Linux app showing side-by-side image comparison and heatmap differences](./image-compare-screenshot.png)

## Features

- Compare two or more images side by side
- Stack images and cycle between them with arrow keys
- Match zoom and pan across images with different dimensions
- Generate perceptual heatmap differences between two same-size images
- Use a grid layout when four or more images are loaded
- Render images faithfully with embedded color profiles, or view them in raw mode
- Open images with drag-and-drop, file picker, command-line arguments, or your file manager’s **Open With** menu
- Load, zoom, and pan large images efficiently
  
## Install

### Flathub

```bash
flatpak install flathub io.github.gimletlove.imagecompare
```

<a href="https://flathub.org/apps/details/io.github.gimletlove.imagecompare">
  <img alt="Download Image Compare on Flathub" src="https://flathub.org/api/badge?svg&locale=en"/>
</a>

### Arch Linux AUR

```bash
yay -S imagecompare-bin
```

## How To Use

- Drag and drop image files into the app, or use **Open** or press `o` in the toolbar.
- Pass image paths on the command line when launching the app.
- Use **Open With** from your file manager, if supported, to open selected images in Image Compare.
- Use **Best Fit** or press `f` to fit images to their viewport.
- Use **Stack** or press `v` to stack images and cycle between them with the arrow keys.
- Use **Match Zoom** or press `h` to normalize zoom and pan across images with different dimensions.
- Use **Faithful / Raw** or press `r` to switch between embedded color-profile rendering and raw display mode.
- Use **Build Heatmap** or press `b` to build a pereceptual heatmap of differences from 2 images with the same dimension.

## Use Cases

Image Compare is useful for:

- Spotting subtle visual changes between image versions
- Spotting subtle differences between different compression methods, such as JXL or AVIF
- Creating a heatmap of differences between images 

## Runtime Requirements

- Qt6 runtime libraries
- KDE Frameworks 6 CoreAddons runtime libraries
- libvips runtime libraries

## Build Requirements

- C++20 compiler
- CMake 3.21+
- Qt6
- KDE Frameworks 6 CoreAddons
- libvips and vips-cpp

Check installed dependency versions:

```bash
pkg-config --modversion Qt6Core
pkg-config --modversion vips
pkg-config --modversion vips-cpp
```

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Run

Open the app:

```bash
./build/imagecompare
```

## Project Links

- Flathub: https://flathub.org/apps/details/io.github.gimletlove.imagecompare
- Arch Linux AUR: https://aur.archlinux.org/packages/imagecompare-bin

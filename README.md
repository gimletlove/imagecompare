# Image Compare - Image Comparison & Visual Diff Tool for Linux and Windows

[![GitHub Release](https://img.shields.io/github/v/release/gimletlove/imagecompare?label=release)](https://github.com/gimletlove/imagecompare/releases)
[![Flathub](https://img.shields.io/flathub/v/io.github.gimletlove.imagecompare?label=Flathub)](https://flathub.org/apps/details/io.github.gimletlove.imagecompare)
[![AUR](https://img.shields.io/aur/version/imagecompare?label=AUR)](https://aur.archlinux.org/packages/imagecompare)
[![License: GPL-3.0](https://img.shields.io/github/license/gimletlove/imagecompare)](./LICENSE.txt)

**Image Compare** is a desktop image comparison and visual diff tool for Linux and Windows. Compare two or more images side by side, in a stacked view, or in a grid layout, and generate luminance SSIM heatmaps to highlight structural differences between two images.

Use it to compare different versions of the same image, choose the best-quality copy from multiple sources, check compression artifacts, inspect scans or image exports, compare edited or upscaled versions, and catch subtle changes in color, sharpness, cropping, scaling, or encoding.

## Screenshot

![Image Compare app showing side-by-side image comparison and heatmap differences](./image-compare-screenshot.png)

## Features

- Open images using drag-and-drop, the file picker, command-line arguments, or a file manager’s **Open With** action
- Compare two or more images side by side, in a stacked view, or in a grid layout
- Synchronize zoom and pan across all images
- Use relative synchronization to keep the same proportional region visible across different resolutions
- Lock individual images to zoom and pan them independently
- Flip between images in the same position using stacked view
- Generate and export luminance SSIM heatmaps to highlight structural differences between two images
- Switch between color-managed rendering using embedded profiles and raw rendering
- Copy image paths, open containing folders, and close files from image context menus

## Keyboard Shortcuts

| Shortcut | Action |
|---|---|
| `o` | Open images |
| `f` | Toggle best fit / 100% zoom |
| `v` | Toggle stacked view |
| Arrow keys | Cycle images in stacked view |
| `Enter` | Focus the active image |
| `h` | Toggle relative sync for different resolutions |
| `l` | Toggle synchronization lock for the active image |
| `r` | Toggle color-profile / raw rendering |
| `b` | Build heatmap |
| `Ctrl+W` | Close active image |
| `Ctrl+Left` / `Ctrl+Right` | Move image left / right |

## Install

### GitHub Releases

Download release builds from the [GitHub Releases page](https://github.com/gimletlove/imagecompare/releases).

Available release builds:

- Linux DEB package for Debian and Ubuntu-based distributions
- Linux RPM package for Fedora and RPM-based distributions
- Linux portable zip
- Windows x86_64 portable zip
- Source tarball

### Flathub

```bash
flatpak install flathub io.github.gimletlove.imagecompare
```

### Arch Linux AUR

```bash
yay -S imagecompare
```

### Windows

Download `imagecompare-<version>-windows-x86_64.zip` from GitHub Releases, extract it, and run `imagecompare.exe`.

The Windows build is distributed as a portable zip with its runtime dependencies bundled.

## Build from Source

Local source builds are intended for Linux. Windows packages are produced by the GitHub Actions release workflow.

Build requirements:

- C++20 compiler
- CMake 3.21+
- Qt 6

Build and run:

```bash
cmake -S . -B build
cmake --build build
./build/imagecompare
```

## Acknowledgements

Luminance SSIM heatmaps use the vendored [rmgr::ssim](https://github.com/romigrou/ssim) library by Romain Bailly.

## Project Links

- GitHub Releases: https://github.com/gimletlove/imagecompare/releases
- Flathub: https://flathub.org/apps/details/io.github.gimletlove.imagecompare
- Arch Linux AUR: https://aur.archlinux.org/packages/imagecompare-bin

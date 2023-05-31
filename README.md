# VGMPlay [![Build Status](https://travis-ci.org/vgmrips/vgmplay.svg?branch=master)](https://travis-ci.org/vgmrips/vgmplay)

The official and always up-to-date player for all [VGM](https://en.wikipedia.org/wiki/VGM_(file_format)) files.

In the future, the existing VGMPlay will be replaced by [libvgm](https://github.com/ValleyBell/libvgm), which is currently in development.

## Contact

* [VGMRips Forums](http://vgmrips.net/forum/index.php)
* IRC: irc.digibase.ca #vgmrips

## Compile VGMPlay under Windows

### Using MS Visual Studio 6.0:

1. Open `VGMPlay.dsw`.
2. Build the project.
3. Done.

### Using later versions of MS Visual Studio:

1. Open `VGMPlay.vcxproj`.
2. Build the project.
3. Done.

### Using MinGW/MSYS:

1. open MSYS and run `make WINDOWS=1` in VGMPlay's folder.
2. Done.

Note: You can compile it without MSYS, but you need to manually create
the OBJDIRS paths (or make them use the backslash '\'), because mkdir fails
at paths with a forward slash.

## Compile VGMPlay under Linux

1. [optional step] If you have libao installed, you can edit the 
Makefile to make VGMPlay use `libao` instead of `OSS`.
2. run `make` in VGMPlay's folder
3. Done. Optionally `sudo make install` and `sudo make play_install`.

### Building on Ubuntu (16.04)

#### Requirements

The following packages are needed in order to compile the binaries

```sh
sudo apt-get install make gcc zlib1g-dev libao-dev libdbus-1-dev
```

#### Building

```sh
make
```

## Compile VGMPlay under macOS

1. install libao by executing the line `brew install libao`
2. run `make install MACOSX=1 DISABLE_HWOPL_SUPPORT=1` in VGMPlay's folder 
(Alternatively edit the Makefile to set those constants and just run `make`.)
3. Done.

Thanks to grauw for macOS compilation instructions.

## Compile VGMPlay under Android
1. Install [Termux](https://github.com/termux/termux-app) on [F-Droid](https://f-droid.org/en/packages/com.termux/) or [GitHub](https://github.com/termux/termux-app/releases). Do not download Termux from Play Store for security and depreciation reasons
2. Open Termux and do `pkg update`
3. When you do pkg update, do `pkg install clang dbus git libao make pkg-config -y`
4. After the installation is done, do `git clone https://github.com/vgmrips/vgmplay`
5. After Done Cloning, do `cd vgmplay/VGMPlay`
6. And then do `make`

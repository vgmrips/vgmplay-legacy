# VGMPlay Build Instructions

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

1. edit the Makefile and enable the line `WINDOWS = 1` (remove the #)
2. open MSYS and run the `make` command in VGMPlay's folder.
3. Done.

Note: You can compile it without MSYS, but you need to manually create
the OBJDIRS paths (or make them use the backslash '\'), because mkdir fails
at paths with a forward slash.

## Compile VGMPlay under Linux

1. [optional step] If you have libao installed, you can edit the 
Makefile to make VGMPlay use `libao` instead of `OSS`.
2. run `make` in VGMPlay's folder
3. Done.

### Building on Ubuntu (16.04)

#### Requirements

The following packages are needed in order to compile the binaries

```sh
sudo apt-get install make gcc zlib1g-dev libao-dev
```

#### Building

```sh
make
```

## Compile VGMPlay under macOS

1. install libao by executing the line `brew install libao`
2. run `make install MACOSX=1 DISABLE_HWOPL_SUPPORT=1` in VGMPlay's folder 
(Alternatively edit the Makefile to set those constants and just run "make".)
3. Done.

Thanks to grauw for macOS compilation instructions.

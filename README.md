# mkdcdisc

A command-line tool to generate Dreamcast-bootable .CDI image files for homebrew games.

This intends to be a one-stop-shop for creating Dreamcast DiscJuggler images, doing what scramble, makeip, objcopy, and cdi4dc do currently but with
additional features like automatic disc padding, and CDDA audio tracks.

## Usage

```
mkdcdisc --help

Usage: mkdcdisc [OPTION]... -e [EXECUTABLE] -o [OUTPUT]
Generate a DiscJuggler .cdi file for use with the SEGA Dreamcast.

  -a, --author                author of the disc/game
  -b, --unscrambled-binary    executable file to use as 1ST_READ.BIN, in unscrambled binary format
  -B, --scrambled-binary      executable file to use as 1ST_READ.BIN, in scrambled binary format
  -c, --cdda                  .wav file to use as an audio track. Specify multiple times to create multiple tracks
  -d, --directory             directory to include (recursively) in the data track. Repeat for multiple directories
  -D, --directory-contents    directory whose contents should be included (recursively) in the data track. Repeat for multiple directories
  -e, --elf                   executable file to use as 1ST_READ.BIN
  -f, --file                  file to include in the data track. Repeat for multiple files
  -h, --help                  this help screen
  -i, --image                 path to a suitable MR format image for the license screen
  -m, --no-mr                 disable the default MR boot image
  -I, --dump-iso              if specified, the data track will be written to a .iso alongside the .cdi
  -o, --output                output filename
  -n, --name                  name of the game (must be fewer than 128 characters)
  -N, --no-padding            specify to disable padding of the data track
  -p, --ipbin                 ip.bin file to use instead of the default one
  -q, --quiet                 disable logging. equivalent to 'v 0'
  -r, --release               release date in YYYYMMDD format
  -s, --serial                disk serial number
  -S, --sort-file             path to sort file
  -v, --verbosity             a number between 0 and 3, 0 == no output
```

## Sort file
When using the -S flag, you must provide a sort file that specifies the order in which files and directories are written to the ISO image. This ordering can influence the performance of data retrieval, where data on the outer edges of the disc can be read faster.

**Understanding the Sort File:**

- **Structure:** Each line in the sort file corresponds to a file or directory path, followed by a weight value. The format is:

```
/path/to/file_or_directory weight
```

**Weights:**
- **Default Weight:** All files/directories are assigned a default value of 0.
- **Positive Weights:** Higher positive values place files closer to the center of the disc.
- **Negative Weights:** Lower negative values place files nearer to the outer edge of the disc for faster read times.
- **Directory Handling:** Assigning a weight to a directory applies that weight to all its contents recursively.
- **Comments and Blank Lines:** Lines starting with `#` are treated as comments and ignored. Blank lines are also ignored.

**Example Sort File:**

```
# Position the introductory video at the outer edge for immediate playback
/media/videos/intro.mp4 -1000

# Place high-resolution images for quick access
/media/images/high_res/image1.jpg -800
/media/images/high_res/image2.jpg -800

# Default weight files are written before the files below

# Standard resolution images can be placed closer to the center
/media/images/standard_res/image3.jpg 200
/media/images/standard_res/image4.jpg 200
```

## Dependencies
- A C++ Compiler
- git
- meson
- libisofs

Fedora: `dnf install git meson gcc gcc-c++ libisofs-devel`

Ubuntu: `sudo apt install git meson build-essential pkg-config libisofs-dev`

MacOS: `brew install git meson pkg-config libisofs`

Windows with MSYS2 (`MSYSTEM=MSYS`):

```
pacman -S --noconfirm git meson gcc autotools libtool make libiconv-devel
curl -LO https://files.libburnia-project.org/releases/libisofs-1.5.6.tar.gz
tar xf libisofs-1.5.6.tar.gz
cd libisofs-1.5.6
./bootstrap
./configure --prefix=/usr --disable-static
sed -i.bak -e "s/allow_undefined=yes/allow_undefined=no/" libtool
(make || (sed -i.bak -e "s/allow_undefined=yes/allow_undefined=no/" libtool && make))
make install
```

Windows with MinGW-w64 without MSYS2 (`MSYSTEM=MINGW64`): `pacman --noconfirm -Syu git mingw-w64-x86_64-autotools mingw-w64-x86_64-gcc mingw-w64-x86_64-libtool mingw-w64-x86_64-make mingw-w64-x86_64-libiconv mingw-w64-x86_64-git mingw-w64-x86_64-meson`

## Download and Build
```
# clone source
git clone [https://github.com/Conkwer/mkdcdisc-windows-binaries/releases](https://github.com/Conkwer/mkdcdisc-windows-binaries)
cd mkdcdisc

meson setup builddir
meson compile -C builddir

# run
./builddir/mkdcdisc -h
```


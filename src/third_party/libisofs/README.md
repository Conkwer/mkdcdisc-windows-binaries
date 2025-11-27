# libisofs for Windows

This is the `libisofs` library from the `libburnia` project, lightly patched for
building and use on Windows.

This source code comes from the [official `libburnia` repository](https://dev.lovelyhq.com/libburnia/libisofs.git).
More information is available in the [git_info.json](git_info.json) file.

If you want to learn more regarding `libisofs`, you can read the official
[README](README) file.

**Note:** This version of the library has only been ported to the `mkdcdisc`
project. It has not been fully tested as a whole. It may or may not work fully.

**Acknowledgments:**

This patched version is based on preliminary work by Unknown Shadow (@UnknownShadow200)
for LXDream, [available here](https://gitlab.com/UnknownShadow200/lxdream-nitro/-/tree/win-fixes?ref_type=heads).

However, the approach is slightly different as the changes in the `libisofs`
files have been made minimally to facilitate updates to this dependency in the
future.

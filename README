# Advanced Topic in Operating Systems

This repo contains solutions for Advanced Topics in Operating System course at MIMUW
http://students.mimuw.edu.pl/ZSO/PUBLIC-SO/2017-2018/_build/html/z2_driver/index-en.html

### Introduction

The task is to write a driver for [the HardDoom™ device](harddoom.html#z2-harddoom), which is a graphics accelerator designed for Doom. The device is delivered in the form of a [modified version of qemu](#z2-qemu-en).

The device should be available to the user in the form of a character device. For each HardDoom™ device present in the system, create a `/dev/doomX` character device, where X is the index of the HardDoom™ device, starting with 0.

### Character device interface

The device `/dev/doom*` is used only to create HardDoom™ resources – all the proper operations will be performed on the created resources. It should support the following operations:

 * `open`: obviously.
 * `close`: obviously.
 * `ioctl(DOOMDEV_IOCTL_CREATE_SURFACE)`: creates a new frame buffer on the device. As a parameter of this call, the dimensions of the buffer (width and height) are transmitted. The width must be a multiple of 64 in the range 64 … 2048 and the height must be in the range 1 … 2048. The result of this call is a new file descriptor referring to the created buffer. The buffer created has undefined content.
 * `ioctl(DOOMDEV_IOCTL_CREATE_TEXTURE)`: creates a new column texture on the device. Parameters of this call are texture size in bytes (maximum 4MiB), texture height in texels (maximum 1023, or 0, if the texture is not to be repeated vertically), and an pointer to the texture data. The result is a file descriptor referring to the texture created.
 * `ioctl(DOOMDEV_IOCTL_CREATE_FLAT)`: creates a new flat texture on the device. The parameter for this call is the data pointer (`0x1000` bytes). The result is a file descriptor referring to the texture created.
 * `ioctl(DOOMDEV_IOCTL_CREATE_COLORMAPS)`: creates a new array of color maps on the device. The parameters of this call are the size of the array (number of color maps) and the pointer to data (each color map is `0x100` bytes). The result is a file descriptor referring to the created array. The maximum allowable size for the array is `0x100` maps.

Textures and color map arrays do not support any standard operations except `close` (which, if all other references have already been released, releases their memory) – they can only be used as parameters for drawing calls. It is also impossible to change their content in any way after creation.

All pointers are passed as `uint64_t` so that the structures have the same layout in 64-bit mode as in 32-bit mode, avoiding the need to define corresponding `_compat` structures. For the same reason, many structures have unused `_pad` fields.

The following operations can be called on the frame buffers:

 * `ioctl (DOOMDEV_SURF_IOCTL_COPY_RECT)`: performs a series of `COPY_RECT` operations to a given buffer. Parameters are:
    * `surf_src_fd`: file descriptor pointing to the frame buffer from which the copy should be made.
    * `rects_ptr`: a pointer to an array of `doomdev_copy_rect` structures.
    * `rects_num`: number of rectangles to copy.
    * in the `doomdev_copy_rect` structures:
        * `pos_dst_x`, `pos_dst_y` – coordinates of the target rectangle in the given buffer (top left corner).
        * `pos_src_x`, `pos_src_y` – coordinates of the source rectangle in the source buffer.
        * `width`, `height` – size of the rectangle to be copied.
    
    The semantics of this call are quite similar to `write`: the driver tries to perform as many operations as possible from the given list, stopping in case of error or signal arrival. If the first operation failed, the error code is returned. Otherwise, the number of completed operations is returned. The user code is responsible for retrying when incomplete.
    
    The user is responsible for ensuring that, within one `ioctl` call, no pixel is both written and read (ie, the command `INTERLOCK` between the rectangles will not be required). The driver does not have to detect such situations (but it can if it wants to) – sending commands to the device and obtaining an incorrect drawing result is acceptable in such a situation.
    
 * `ioctl(DOOMDEV_SURF_IOCTL_FILL_RECT)`: performs a series of `FILL_RECT` operations. Parameters:
    * `rects_ptr`: a pointer to an array of `doomdev_fill_rect` structures.
    * `rects_num`: number of rectangles to fill.
    * in the `doomdev_fill_rect` structures:
        * `pos_dst_x`, `pos_dst_y` – coordinates of the target rectangle in the given buffer.
        * `width`, `height` – size of the rectangle to be filled.
        * `color` – the fill color.
    
    The returned value is as in `DOOMDEV_SURF_IOCTL_COPY_RECT`.
    
* `ioctl(DOOMDEV_SURF_IOCTL_DRAW_LINE)`: performs a series of `DRAW_LINE` operations. Parameters:
    * `lines_ptr`: a pointer to an array of `doomdev_line` structures.
    * `lines_num`: number of lines to draw.
    * in the `doomdev_line` structures:
        * `pos_a_x`, `pos_a_y`: coordinates of the first endpoint of the line.
        * `pos_b_x`, `pos_b_y`: coordinates of the second endpoint.
        * `color` – the color of the line to be drawn.
    
    The returned value is as in `DOOMDEV_SURF_IOCTL_COPY_RECT`.
    
* `ioctl(DOOMDEV_SURF_IOCTL_DRAW_BACKGROUND)`: performs the `DRAW_BACKGROUND` operation. Parameters:
    * `flat_fd`: a file descriptor pointing to a flat texture that will serve as the background.
    
    In case of a successful call, 0 is returned.
    
* `ioctl(DOOMDEV_SURF_IOCTL_DRAW_COLUMNS)`: performs a series of `DRAW_COLUMN` operations. Parameters:
    * `draw_flags`: a combination of the following flags:
        * `DOOMDEV_DRAW_FLAGS_FUZZ` – if set, the fuzz effect will be rendered – most parameters are ignored (including other flags).
        * `DOOMDEV_DRAW_FLAGS_TRANSLATE` – if set, the palette will be remapped according to the translation color map.
        * `DOOMDEV_DRAW_FLAGS_COLORMAP` – if set, colors will be dimmed according to the color map.
    * `texture_fd`: a descriptor of the column texture (ignored if the `FUZZ` flag is set).
    * `translation_fd`: a descriptor of the color map array used by the `TRANSLATE` flag (ignored, if the flag is not set).
    * `colormap_fd`: a descriptor of the color map array used by the `COLORMAP` and `FUZZ` flags. Ignored, if none of these flags is set.
    * `translation_idx`: index of the color map used by the `TRANSLATE` option. Used only, if the `TRANSLATE` flag is set.
    * `columns_num`: number of columns to draw.
    * `columns_ptr`: a pointer to an array of `doomdev_column` structures:
        * `column_offset`: starting offset of this column in the texture.
        * `ustart`: an unsigned fixed-point 16.16 number, must be in the range supported by the hardware. Ignored, if the `FUZZ` flag is used.
        * `ustep`: an unsigned fixed-point 16.16 number, must be in the range supported by the hardware. Ignored, if the `FUZZ` flag is used.
        * `x`: the `x` coordinate of the column.
        * `y1`, `y2`: the `y` coordinates of the top and bottom pixels of the column.
        * `colormap_idx`: index of the color map used by `FUZZ` and `COLORMAP` flags. Ignored, if neither of those is set.
    
    The returned value is as in `DOOMDEV_SURF_IOCTL_COPY_RECT`.
    
* `ioctl(DOOMDEV_SURF_IOCTL_DRAW_SPANS)`: performs a series of `DRAW_SPAN` operations. Parameters:
    * `flat_fd`: a flat texture descriptor.
    * `translation_fd`: like above.
    * `colormap_fd`: like above.
    * `draw_flags`: like above, but without the `FUZZ` flag.
    * `translation_idx`: like above.
    * `spans_num`: number of spans to draw.
    * `spans_ptr` a pointer to an array of `doomdev_span` structures:
        * `ustart`, `vstart`: like `ustart` above.
        * `ustep`, `vstep`: like `ustep` above.
        * `x1`, `x2`: the `x` coordinates of the leftmost and rightmost pixel of the span.
        * `y`: the `y` coordinate of the span.
        * `colormap_idx`: like above.
    
    The returned value is as in `DOOMDEV_SURF_IOCTL_COPY_RECT`.
* `lseek`: sets the position in the buffer for subsequent `read` calls.
    
* `read`, `pread`, `readv`, etc: waits for completion of all previously submitted drawing operations for the given buffer, and then reads the finished data from the buffer to the user space. In case of an attempt to read outside of buffer bounds, end-of-file should be returned.
    

The driver should detect commands with incorrect parameters (wrong file type passed as `*_fd`, coordinates extending beyond the frame buffer, etc.) and return the error `EINVAL`. If the user tries to create textures or frame buffers larger than those supported by the hardware, `EOVERFLOW` should be returned.

The driver should register its devices in sysfs so that udev automatically creates device files with appropriate names in `/dev`. The major and minor numbers for these devices are arbitrary (majors should be allocated dynamically).

A header file with the appropriate definitions can be found here: [https://github.com/koriakin/prboom-plus/blob/doomdev/src/doomdev.h](https://github.com/koriakin/prboom-plus/blob/doomdev/src/doomdev.h)

The driver can assume a limit of 256 devices in the system.

### Assumptions for interaction with hardware

It can be assumed that before the driver is loaded, the device has a state like a hardware reset. The device should also be left in this state when the driver is unloaded.

A fully-scored solution should work asynchronously – drawing `ioctl` operations should send commands to the device and return to the user space without waiting for completion (but if the command buffers are already full, it is acceptable to wait for free space to become available). Waiting for the end of the command should only be done when calling `read` which will actually need the drawing results.




-----------------------------------



        Linux kernel release 4.x <http://kernel.org/>

These are the release notes for Linux version 4.  Read them carefully,
as they tell you what this is all about, explain how to install the
kernel, and what to do if something goes wrong.

WHAT IS LINUX?

  Linux is a clone of the operating system Unix, written from scratch by
  Linus Torvalds with assistance from a loosely-knit team of hackers across
  the Net. It aims towards POSIX and Single UNIX Specification compliance.

  It has all the features you would expect in a modern fully-fledged Unix,
  including true multitasking, virtual memory, shared libraries, demand
  loading, shared copy-on-write executables, proper memory management,
  and multistack networking including IPv4 and IPv6.

  It is distributed under the GNU General Public License - see the
  accompanying COPYING file for more details.

ON WHAT HARDWARE DOES IT RUN?

  Although originally developed first for 32-bit x86-based PCs (386 or higher),
  today Linux also runs on (at least) the Compaq Alpha AXP, Sun SPARC and
  UltraSPARC, Motorola 68000, PowerPC, PowerPC64, ARM, Hitachi SuperH, Cell,
  IBM S/390, MIPS, HP PA-RISC, Intel IA-64, DEC VAX, AMD x86-64, AXIS CRIS,
  Xtensa, Tilera TILE, AVR32, ARC and Renesas M32R architectures.

  Linux is easily portable to most general-purpose 32- or 64-bit architectures
  as long as they have a paged memory management unit (PMMU) and a port of the
  GNU C compiler (gcc) (part of The GNU Compiler Collection, GCC). Linux has
  also been ported to a number of architectures without a PMMU, although
  functionality is then obviously somewhat limited.
  Linux has also been ported to itself. You can now run the kernel as a
  userspace application - this is called UserMode Linux (UML).

DOCUMENTATION:

 - There is a lot of documentation available both in electronic form on
   the Internet and in books, both Linux-specific and pertaining to
   general UNIX questions.  I'd recommend looking into the documentation
   subdirectories on any Linux FTP site for the LDP (Linux Documentation
   Project) books.  This README is not meant to be documentation on the
   system: there are much better sources available.

 - There are various README files in the Documentation/ subdirectory:
   these typically contain kernel-specific installation notes for some
   drivers for example. See Documentation/00-INDEX for a list of what
   is contained in each file.  Please read the Changes file, as it
   contains information about the problems, which may result by upgrading
   your kernel.

 - The Documentation/DocBook/ subdirectory contains several guides for
   kernel developers and users.  These guides can be rendered in a
   number of formats:  PostScript (.ps), PDF, HTML, & man-pages, among others.
   After installation, "make psdocs", "make pdfdocs", "make htmldocs",
   or "make mandocs" will render the documentation in the requested format.

INSTALLING the kernel source:

 - If you install the full sources, put the kernel tarball in a
   directory where you have permissions (e.g. your home directory) and
   unpack it:

     xz -cd linux-4.X.tar.xz | tar xvf -

   Replace "X" with the version number of the latest kernel.

   Do NOT use the /usr/src/linux area! This area has a (usually
   incomplete) set of kernel headers that are used by the library header
   files.  They should match the library, and not get messed up by
   whatever the kernel-du-jour happens to be.

 - You can also upgrade between 4.x releases by patching.  Patches are
   distributed in the xz format.  To install by patching, get all the
   newer patch files, enter the top level directory of the kernel source
   (linux-4.X) and execute:

     xz -cd ../patch-4.x.xz | patch -p1

   Replace "x" for all versions bigger than the version "X" of your current
   source tree, _in_order_, and you should be ok.  You may want to remove
   the backup files (some-file-name~ or some-file-name.orig), and make sure
   that there are no failed patches (some-file-name# or some-file-name.rej).
   If there are, either you or I have made a mistake.

   Unlike patches for the 4.x kernels, patches for the 4.x.y kernels
   (also known as the -stable kernels) are not incremental but instead apply
   directly to the base 4.x kernel.  For example, if your base kernel is 4.0
   and you want to apply the 4.0.3 patch, you must not first apply the 4.0.1
   and 4.0.2 patches. Similarly, if you are running kernel version 4.0.2 and
   want to jump to 4.0.3, you must first reverse the 4.0.2 patch (that is,
   patch -R) _before_ applying the 4.0.3 patch. You can read more on this in
   Documentation/applying-patches.txt

   Alternatively, the script patch-kernel can be used to automate this
   process.  It determines the current kernel version and applies any
   patches found.

     linux/scripts/patch-kernel linux

   The first argument in the command above is the location of the
   kernel source.  Patches are applied from the current directory, but
   an alternative directory can be specified as the second argument.

 - Make sure you have no stale .o files and dependencies lying around:

     cd linux
     make mrproper

   You should now have the sources correctly installed.

SOFTWARE REQUIREMENTS

   Compiling and running the 4.x kernels requires up-to-date
   versions of various software packages.  Consult
   Documentation/Changes for the minimum version numbers required
   and how to get updates for these packages.  Beware that using
   excessively old versions of these packages can cause indirect
   errors that are very difficult to track down, so don't assume that
   you can just update packages when obvious problems arise during
   build or operation.

BUILD directory for the kernel:

   When compiling the kernel, all output files will per default be
   stored together with the kernel source code.
   Using the option "make O=output/dir" allows you to specify an alternate
   place for the output files (including .config).
   Example:

     kernel source code: /usr/src/linux-4.X
     build directory:    /home/name/build/kernel

   To configure and build the kernel, use:

     cd /usr/src/linux-4.X
     make O=/home/name/build/kernel menuconfig
     make O=/home/name/build/kernel
     sudo make O=/home/name/build/kernel modules_install install

   Please note: If the 'O=output/dir' option is used, then it must be
   used for all invocations of make.

CONFIGURING the kernel:

   Do not skip this step even if you are only upgrading one minor
   version.  New configuration options are added in each release, and
   odd problems will turn up if the configuration files are not set up
   as expected.  If you want to carry your existing configuration to a
   new version with minimal work, use "make oldconfig", which will
   only ask you for the answers to new questions.

 - Alternative configuration commands are:

     "make config"      Plain text interface.

     "make menuconfig"  Text based color menus, radiolists & dialogs.

     "make nconfig"     Enhanced text based color menus.

     "make xconfig"     Qt based configuration tool.

     "make gconfig"     GTK+ based configuration tool.

     "make oldconfig"   Default all questions based on the contents of
                        your existing ./.config file and asking about
                        new config symbols.

     "make silentoldconfig"
                        Like above, but avoids cluttering the screen
                        with questions already answered.
                        Additionally updates the dependencies.

     "make olddefconfig"
                        Like above, but sets new symbols to their default
                        values without prompting.

     "make defconfig"   Create a ./.config file by using the default
                        symbol values from either arch/$ARCH/defconfig
                        or arch/$ARCH/configs/${PLATFORM}_defconfig,
                        depending on the architecture.

     "make ${PLATFORM}_defconfig"
                        Create a ./.config file by using the default
                        symbol values from
                        arch/$ARCH/configs/${PLATFORM}_defconfig.
                        Use "make help" to get a list of all available
                        platforms of your architecture.

     "make allyesconfig"
                        Create a ./.config file by setting symbol
                        values to 'y' as much as possible.

     "make allmodconfig"
                        Create a ./.config file by setting symbol
                        values to 'm' as much as possible.

     "make allnoconfig" Create a ./.config file by setting symbol
                        values to 'n' as much as possible.

     "make randconfig"  Create a ./.config file by setting symbol
                        values to random values.

     "make localmodconfig" Create a config based on current config and
                           loaded modules (lsmod). Disables any module
                           option that is not needed for the loaded modules.

                           To create a localmodconfig for another machine,
                           store the lsmod of that machine into a file
                           and pass it in as a LSMOD parameter.

                   target$ lsmod > /tmp/mylsmod
                   target$ scp /tmp/mylsmod host:/tmp

                   host$ make LSMOD=/tmp/mylsmod localmodconfig

                           The above also works when cross compiling.

     "make localyesconfig" Similar to localmodconfig, except it will convert
                           all module options to built in (=y) options.

   You can find more information on using the Linux kernel config tools
   in Documentation/kbuild/kconfig.txt.

 - NOTES on "make config":

    - Having unnecessary drivers will make the kernel bigger, and can
      under some circumstances lead to problems: probing for a
      nonexistent controller card may confuse your other controllers

    - A kernel with math-emulation compiled in will still use the
      coprocessor if one is present: the math emulation will just
      never get used in that case.  The kernel will be slightly larger,
      but will work on different machines regardless of whether they
      have a math coprocessor or not.

    - The "kernel hacking" configuration details usually result in a
      bigger or slower kernel (or both), and can even make the kernel
      less stable by configuring some routines to actively try to
      break bad code to find kernel problems (kmalloc()).  Thus you
      should probably answer 'n' to the questions for "development",
      "experimental", or "debugging" features.

COMPILING the kernel:

 - Make sure you have at least gcc 3.2 available.
   For more information, refer to Documentation/Changes.

   Please note that you can still run a.out user programs with this kernel.

 - Do a "make" to create a compressed kernel image. It is also
   possible to do "make install" if you have lilo installed to suit the
   kernel makefiles, but you may want to check your particular lilo setup first.

   To do the actual install, you have to be root, but none of the normal
   build should require that. Don't take the name of root in vain.

 - If you configured any of the parts of the kernel as `modules', you
   will also have to do "make modules_install".

 - Verbose kernel compile/build output:

   Normally, the kernel build system runs in a fairly quiet mode (but not
   totally silent).  However, sometimes you or other kernel developers need
   to see compile, link, or other commands exactly as they are executed.
   For this, use "verbose" build mode.  This is done by passing
   "V=1" to the "make" command, e.g.

     make V=1 all

   To have the build system also tell the reason for the rebuild of each
   target, use "V=2".  The default is "V=0".

 - Keep a backup kernel handy in case something goes wrong.  This is
   especially true for the development releases, since each new release
   contains new code which has not been debugged.  Make sure you keep a
   backup of the modules corresponding to that kernel, as well.  If you
   are installing a new kernel with the same version number as your
   working kernel, make a backup of your modules directory before you
   do a "make modules_install".

   Alternatively, before compiling, use the kernel config option
   "LOCALVERSION" to append a unique suffix to the regular kernel version.
   LOCALVERSION can be set in the "General Setup" menu.

 - In order to boot your new kernel, you'll need to copy the kernel
   image (e.g. .../linux/arch/x86/boot/bzImage after compilation)
   to the place where your regular bootable kernel is found.

 - Booting a kernel directly from a floppy without the assistance of a
   bootloader such as LILO, is no longer supported.

   If you boot Linux from the hard drive, chances are you use LILO, which
   uses the kernel image as specified in the file /etc/lilo.conf.  The
   kernel image file is usually /vmlinuz, /boot/vmlinuz, /bzImage or
   /boot/bzImage.  To use the new kernel, save a copy of the old image
   and copy the new image over the old one.  Then, you MUST RERUN LILO
   to update the loading map! If you don't, you won't be able to boot
   the new kernel image.

   Reinstalling LILO is usually a matter of running /sbin/lilo.
   You may wish to edit /etc/lilo.conf to specify an entry for your
   old kernel image (say, /vmlinux.old) in case the new one does not
   work.  See the LILO docs for more information.

   After reinstalling LILO, you should be all set.  Shutdown the system,
   reboot, and enjoy!

   If you ever need to change the default root device, video mode,
   ramdisk size, etc.  in the kernel image, use the 'rdev' program (or
   alternatively the LILO boot options when appropriate).  No need to
   recompile the kernel to change these parameters.

 - Reboot with the new kernel and enjoy.

IF SOMETHING GOES WRONG:

 - If you have problems that seem to be due to kernel bugs, please check
   the file MAINTAINERS to see if there is a particular person associated
   with the part of the kernel that you are having trouble with. If there
   isn't anyone listed there, then the second best thing is to mail
   them to me (torvalds@linux-foundation.org), and possibly to any other
   relevant mailing-list or to the newsgroup.

 - In all bug-reports, *please* tell what kernel you are talking about,
   how to duplicate the problem, and what your setup is (use your common
   sense).  If the problem is new, tell me so, and if the problem is
   old, please try to tell me when you first noticed it.

 - If the bug results in a message like

     unable to handle kernel paging request at address C0000010
     Oops: 0002
     EIP:   0010:XXXXXXXX
     eax: xxxxxxxx   ebx: xxxxxxxx   ecx: xxxxxxxx   edx: xxxxxxxx
     esi: xxxxxxxx   edi: xxxxxxxx   ebp: xxxxxxxx
     ds: xxxx  es: xxxx  fs: xxxx  gs: xxxx
     Pid: xx, process nr: xx
     xx xx xx xx xx xx xx xx xx xx

   or similar kernel debugging information on your screen or in your
   system log, please duplicate it *exactly*.  The dump may look
   incomprehensible to you, but it does contain information that may
   help debugging the problem.  The text above the dump is also
   important: it tells something about why the kernel dumped code (in
   the above example, it's due to a bad kernel pointer). More information
   on making sense of the dump is in Documentation/oops-tracing.txt

 - If you compiled the kernel with CONFIG_KALLSYMS you can send the dump
   as is, otherwise you will have to use the "ksymoops" program to make
   sense of the dump (but compiling with CONFIG_KALLSYMS is usually preferred).
   This utility can be downloaded from
   ftp://ftp.<country>.kernel.org/pub/linux/utils/kernel/ksymoops/ .
   Alternatively, you can do the dump lookup by hand:

 - In debugging dumps like the above, it helps enormously if you can
   look up what the EIP value means.  The hex value as such doesn't help
   me or anybody else very much: it will depend on your particular
   kernel setup.  What you should do is take the hex value from the EIP
   line (ignore the "0010:"), and look it up in the kernel namelist to
   see which kernel function contains the offending address.

   To find out the kernel function name, you'll need to find the system
   binary associated with the kernel that exhibited the symptom.  This is
   the file 'linux/vmlinux'.  To extract the namelist and match it against
   the EIP from the kernel crash, do:

     nm vmlinux | sort | less

   This will give you a list of kernel addresses sorted in ascending
   order, from which it is simple to find the function that contains the
   offending address.  Note that the address given by the kernel
   debugging messages will not necessarily match exactly with the
   function addresses (in fact, that is very unlikely), so you can't
   just 'grep' the list: the list will, however, give you the starting
   point of each kernel function, so by looking for the function that
   has a starting address lower than the one you are searching for but
   is followed by a function with a higher address you will find the one
   you want.  In fact, it may be a good idea to include a bit of
   "context" in your problem report, giving a few lines around the
   interesting one.

   If you for some reason cannot do the above (you have a pre-compiled
   kernel image or similar), telling me as much about your setup as
   possible will help.  Please read the REPORTING-BUGS document for details.

 - Alternatively, you can use gdb on a running kernel. (read-only; i.e. you
   cannot change values or set break points.) To do this, first compile the
   kernel with -g; edit arch/x86/Makefile appropriately, then do a "make
   clean". You'll also need to enable CONFIG_PROC_FS (via "make config").

   After you've rebooted with the new kernel, do "gdb vmlinux /proc/kcore".
   You can now use all the usual gdb commands. The command to look up the
   point where your system crashed is "l *0xXXXXXXXX". (Replace the XXXes
   with the EIP value.)

   gdb'ing a non-running kernel currently fails because gdb (wrongly)
   disregards the starting offset for which the kernel is compiled.


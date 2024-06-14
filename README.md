<p>
 A pure C library that provides quite unified api to develop cross-platform applications, net/web services and so on.<br>
 This is kind of combination of some ideas from the libraries such as: apache apr, libre and other, but with my needs and view on how it shout be.<br>
 The main goals is to have an easy way to develop and maintenance applications on the legacy system, such as: OS/2, Win2k3 or Solaris 10, <br>
 but the same time supports the modern systems with the latest gcc/clang and keep the features of the new standards (and use them by needs). <br>
 Actually, for a long time it was just like a set of files which I included in different projects, but recently time has appeared to arrange it as a library. <br>
</p>

## Version 1.2.0a (14.06.2024)
 - supported systems (which were tested):
    - OS/2 Warp4, eComstation 2.x and others if you manage to install: emx and gcc-3.2x, binutils-2x, gnu make 3x <br>
    - Windows2003 (and other) MinGw-2.4 (gcc-6.3, binutils-2.x, gnu make 3.8) and if you want to have networking support, your winsock version should be not lower 2.2 <br>
    - Windows10 with the latest MSYS2 <br>
    - Solaris10/11 (gcc-5.5, binutils-2x, gnu make 4.x), actually was tested only on Sol10 <br>
    - FreeBSD-12 (clang-10x, gnu make-4.3) and other from the BSD family (NetBSD, OpenBSD, ...) <br>
    - Linux ubuntu server 22.04 (gcc-11x, gnu make-4.3) <br>
    - OpenWRT (*) <br>

Actually it should work without any problem on platforms which meet the following conditions: gcc >= 3.2, gnu make >= 3.x, binutils-2.x <br>

Capabilities: <br>
 - threads/mutexes/sleeps - available on the whole systems, and contains special tools, such as: queue, worker (see: examples/test-queue.c, examples/test-worker.c)<br>
 - networking, besides the low level access to the sockets and its operations also contains advanced multithreaded tcp/udp servers <br>
 - polling, supports the following methods: select, epoll and kqueue <br>
 - loadable modules, capable to works with so/dll libraryes <br>
 - includes structures such as: hashtable, linked list (see: examples/test-hashtable.c, exampes/test-list.c)<br>
 - various operations with the strings: concat, replace, and so on (see: wstk-str.h) <br>
 - some useful things was adopted from the libre, such as: mbuf, fmt, pl, regex, and so on <br>
 - has a builtin multi-threaded http server with the expansion of functionality through 'servlets' (see: examples/test-httpd-servlets.c) <br>
   servlets: <br>
    - servlet-jsonrpc - provides an easy way to write services (see: examples/test-httpd-jsonrpc.c) <br>
    - servlet-websock - provides an api use server websockets  (see: examples/test-httpd-websock.c) <br>
    - servlet-upload  - a simple way to upload files in the server (see: examples/test-httpd-upload.c) <br>
 - on the modern systems (C11 standard) provides api to the atomic operations <br>
 - ssl suppors ( todo ) <br>
 - and other functionality, see examples... <br>

### Installation and build
The library doesn't contain any autotool scripts and so on, all the configurtion options defines in the main Makefile and wstk-core.h (as the preprocessor variables). <br>
To build it is quite enough to perform: 'make clean all', and then manually copying the library file and headers to the desired location. <br>


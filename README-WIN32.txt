
                        UniK OLSR for Windows
                        ---------------------

Welcome to the first Windows release. Let's have a quick look at how
this version is different from the original Linux version.

                        ***** Stability *****

  While the Windows version looks pretty stable in basic scenarios, it
  hasn't yet received the extensive testing by the OLSR community that
  the Linux version has gone through. So, if you experience any
  strange behaviour, it's probably my fault. In this case please bear
  with me and send me a bug report via the OLSR-users mailing
  list. I'll then do my best to find and fix the problem with your
  assistance.

                    ***** Configuration file *****

  If you do not specify a configuration file, the OLSR server by
  default attempts to use "olsrd.conf" in your Windows directory,
  e.g. "C:\WINDOWS\olsrd.conf".

                     ***** Interface naming *****

  On Linux network interfaces have nice names like "eth0". In
  contrast, Windows internally identifies network interfaces by pretty
  awkward names, for example:

    "{EECD2AB6-C2FC-4826-B92E-CAA53B29D67C}"

  Hence, the Windows version implements its own naming scheme that
  maps each internal name to a made-up name like "if03", which is
  easier to memorize. Simply invoke the OLSR executable as follows to
  obtain the OLSR server's view of your interfaces:

    olsrd.exe -int

  This lists the made-up interface names along with their current IP
  addresses to enable you to find out which made-up interface name
  belongs to which of your physical interfaces.

  For techies: The made-up names consist of the string "if" followed
  by a two-digit hex representation of the least significant byte of
  the Windows-internal interface index, which should be different for
  each interface and thus make each made-up name unique. Again, this
  is undocumented and this assumption may be wrong in certain
  cases. So, if the "-int" option reports two interfaces that have the
  same name, please do let me know.

                     ***** Running the GUI *****

  The GUI is a GTK+ application and thus requires the GTK+ runtime
  environment to work. The Windows port of GTK+ is available from the
  following URL:

    http://www.gimp.org/~tml/gimp/win32/downloads.html

  You need to download the following packages or newer versions of
  these packages:

    - atk-1.6.0.zip
    - gettext-runtime-0.13.1.zip
    - glib-2.4.5.zip
    - gtk+-2.4.7.zip
    - libiconv-1.9.1.bin.woe32.zip
    - pango-1.4.1.zip

  Simply unzip the files into a directory that you've created,
  e.g. into "GUI". Then copy the "olsrd-gui.exe" file into the "bin"
  subdirectory, e.g. into "GUI\bin", and run it from there.

                     ***** Missing features *****

  The Windows version currently does not implement the following
  features known from the Linux release.

    * IPv6. This will probably be in version 0.4.7.

    * Link layer statistics.

    * WLAN interface detection. The Windows port does not recognize
      whether an interface is a WLAN interface or a wired LAN
      interface.  All specified interfaces are assumed to be WLAN
      interfaces. So, for example, specifying a different HELLO
      interval for wired interfaces does not currently work. Instead,
      the HELLO interval for WLAN interfaces is always used.

    * Gateway tunnelling. This is currently experimental on
      Windows. It is intended to work reliably on Windows 2000 and
      Windows XP in version 0.4.7. It is based on the ipinip.sys
      device driver that comes with these operating systems, but which
      is completely undocumented. I've figured out how to use the
      device driver, but it looks like I've still missed one or two
      little things. So, tunnelling might work on your OS version, but
      it might as well not work. Unfortunately, currently I do not
      even know why it works on some systems and fails on other
      systems.

      If you are brave, do the following, but be prepared for a BSOD
      (blue screen of death) as a worst-case scenario. This is nothing
      for the faint of the heart. :-) Never try this on production
      systems.

        * Start the IP-in-IP tunnel driver before running the OLSR
          server:

            net start ipinip

        * When the OLSR server reports that the tunnel has been
          established, find out, which interface index the tunnel
          device has been assigned:

            route print

        * Let's assume that the interface index is 0x1234 and the
          gateway's IP address is 1.2.3.4. Manually add a default
          route through the other end of the tunnel:

            route add 0.0.0.0 mask 0.0.0.0 1.2.3.4 if 0x1234

        * Try to ping somebody beyond the gateway and let me know
          whether it works. If it doesn't and if you have time, please
          do a packet dump for me to determine whether IP-in-IP
          packets are leaving your system and, if yes, what they look
          like.

      If you know of any freely available tunnel driver for Windows,
      please let me know. We could then think about switching from the
      native ipinip.sys driver to an alternative driver, perhaps one
      that also works on Windows 9x.

      If you are the Microsoft person that is responsible for the
      tunnel driver, please have a look at my code in
      src/win32/tunnel.* and tell me what I'm missing.

    * Multiple interfaces in the same subnet. As they all share the
      same subnet broadcast address, there's no way to tell Windows
      which of these interfaces to send OLSR packets through. I guess
      that we'll have to come up with a device driver that sits
      between the TCP/IP stack and the network adapters and that
      directs outbound OLSR packets to the correct interface after
      they've been routed by the TCP/IP stack. Looks like there isn't
      any other solution on Windows.

  There are also some Windows-specific features that I currently work
  on, but which have not made it into this release.

    * A nice installation package based on Inno Setup 4. However,
      there will always be a ZIP archive available, too, for those who
      do not like installation packages.

    * The option to run the OLSR server as a Windows service on
      Windows NT, 2000, and XP.

  These features will probably be in 0.4.7.

                        ***** Compiling *****

  To compile the Windows version you need a Cygwin installation with a
  current version of GCC and Mingw32. Currently the OLSR server, the
  GUI, and the dot_draw plugin compile. Each of the corresponding
  subdirectories contains a shell script named "mkmf.sh" that takes
  "Makefile.win32.in" as its input, appends the dependencies, and
  outputs "Makefile.win32". Then simply say

    make -f Makefile.win32 clean

  to remove any compiled files or

    make -f Makefile.win32 mclean

  to remove any compiled files and the generated makefile. Say

    make -f Makefile.win32

  to compile the source code.

  To compile the GUI you additionally need the development packages of
  the GTK+ Windows port. They are available from the same URL as the
  GTK+ runtime environment. You need the following packages or newer
  versions of these packages:

    - atk-dev-1.6.0.zip
    - glib-dev-2.4.5.zip
    - gtk+-dev-2.4.7.zip
    - pango-dev-1.4.1.zip

  Simply unzip the files into a directory that you've created and make
  the variable "GTKBASE" in "Makefile.win32" point to this directory.

Well, thanks for using the initial release of a piece of software and
please bear with me if there are any problems. Please do also feel
free to suggest any features that you'd like to see in future
releases.

Thomas Lopatic <thomas@lopatic.de>, 2004-08-24

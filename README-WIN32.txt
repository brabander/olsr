
                         olsr.org for Windows
                         --------------------

Welcome to the Windows release of olsr.org. Let's have a quick look at
how this version differs from the original Linux version.


                        ***** Stability *****

  While the Windows version looks pretty stable in basic scenarios, it
  hasn't yet received the extensive testing by the OLSR community that
  the Linux version has gone through. So, if you experience any
  strange behaviour, it's probably my fault. In this case please bear
  with me and use the issue tracker on SourceForge. I'll then do my
  best to find and fix the problem with your assistance. The
  SourceForge homepage for olsrd.org has the following URL.

                http://sourceforge.net/projects/olsrd/


                    ***** Configuration file *****

  If you do not specify a configuration file, the OLSR server
  ("olsrd.exe") by default attempts to use "olsrd.conf" in your
  Windows directory, e.g. "C:\WINDOWS\olsrd.conf".


                     ***** Interface naming *****

  On Linux network interfaces have nice names like "eth0". In
  contrast, Windows internally identifies network interfaces by pretty
  awkward names, for example:

    "{EECD2AB6-C2FC-4826-B92E-CAA53B29D67C}"

  Hence, the Windows version implements its own naming scheme that
  maps each internal name to a made-up name like "if03", which is
  easier to memorize. Simply invoke the OLSR server as follows to
  obtain its view of your interfaces:

    olsrd.exe -int

  This lists the made-up interface names along with their current IP
  addresses to enable you to find out which made-up interface name
  corresponds to which of your physical interfaces.

  "+" in front of the IP addresses means that the OLSR server has
  identified the interface as a WLAN interface. "-" indicates that the
  OLSR server considers this interface to be a wired interface. "?"
  means "no idea". Detection currently only works on NT, 2000, and
  XP. Windows 9x and ME will always display "?".

  For techies: The made-up names consist of the string "if" followed
  by a two-digit hex representation of the least significant byte of
  the Windows-internal interface index, which should be different for
  each interface and thus make each made-up name unique. Again, this
  is undocumented and this assumption may be wrong in certain
  cases. So, if the "-int" option reports two interfaces that have the
  same name, please do let me know.


                     ***** Running the GUI *****

  We now have a native Windows GUI. No more GTK+. Simply make sure
  that "Switch.exe", "Shim.exe", and "olsrd.exe" are located in the
  same directory and run "Switch.exe". "Shim.exe" is just an auxiliary
  console application that is required by "Switch.exe".

  The GUI is pretty self-explanatory. The three buttons on the lower
  right of the GUI window start the OLSR server, stop the OLSR server,
  and exit the GUI.

  Use the "Settings" tab to specify the options that the GUI uses to
  run the OLSR server "olsrd.exe". When you click "Start" the GUI
  generates a temporary configuration file from the information given
  by the "Settings" tab. This temporary configuration file is passed
  to the OLSR server via its "-f" option. If you need options that
  cannot be controlled via the "Settings" tab, simply add them to the
  "Manual additions" text box as you would add them to a configuration
  file, e.g. "HNA 192.168.0.0 255.255.255.0". The contents of this
  text box are appended to the temporary configuration file when it is
  generated.

  "Offer Internet connection" is only available if you have an
  Internet connection, i.e. if you have a default route configured. If
  you tick this option, "HNA 0.0.0.0 0.0.0.0" is added to the
  temporary configuration file, allowing other nodes in the OLSR
  network to use your Internet connection.

  Gateway tunnelling and IP version 6 cannot currently be selected, as
  support for these features is not yet complete in the Windows
  version.

  The three buttons on the lower right of the "Settings" tab open
  previously saved settings, save the current settings to a
  configuration file, and reset the current settings to default
  values. When opening a saved configuration file, the GUI adds lines
  that it cannot interpret to the "Manual additions" text box.

  If you start the GUI with the path to a configuration file as the
  only command line argument, the GUI opens the given configuration
  file and runs the OLSR server with this configuration. So, saving a
  configuration file with a ".olsr" extension, for example, and making
  "Switch.exe" the handler for ".olsr" files enables you to run the
  OLSR server with a simple double click on the configuration file.

  The "Output" tab shows the output of the currently running OLSR
  server. When you click "Start" The GUI simply invokes the OLSR
  server "olsrd.exe" and intercepts its console output. Use the four
  buttons on the upper right of the tab to freeze the output, resume
  frozen output, save the output to a file, or clear the output.

  The "Nodes" tab contains information about the nodes that the OLSR
  server currently knows about. If you click on the address of a node
  in the "Node list" list box, the GUI populates the three "Node
  information" list boxes on the right with the multi-point relays of
  the selected node (MPR), the interfaces of the selected node (MID),
  and the non-OLSR networks accessible via the selected node (HNA).

  The "Routes" tab shows the routes that the currently running OLSR
  server has added.


                   ***** Running the GTK+ GUI *****

  Please use the native Windows GUI instead. The GTK+ GUI is no longer
  supported on Windows.


                     ***** Missing features *****

  The Windows version currently does not implement the following
  features known from the Linux release.

    * IPv6.

    * Link layer statistics.

    * Gateway tunnelling.

  There are also some Windows-specific features that I currently work
  on, but which have not made it into this release.

    * The option to run the OLSR server as a Windows service on
      Windows NT, 2000, and XP.


                        ***** Compiling *****

  To compile the Windows version of the OLSR server or the dot_draw
  plugin you need a Cygwin installation with a current version of GCC
  and Mingw32. Each of the corresponding subdirectories contains a
  shell script named "mkmf.sh" that takes "Makefile.win32.in" as its
  input, appends the dependencies, and outputs "Makefile.win32". Then
  simply say

    make -f Makefile.win32 clean

  to remove any compiled files or

    make -f Makefile.win32 mclean

  to remove any compiled files and the generated makefile. Say

    make -f Makefile.win32

  to compile the source code.

  The GUI has to be compiled with Visual C++ 6. Simply open the
  "Frontend.dsw" workspace in the Visual C++ 6 IDE. Then compile
  "Frontend" and "Shim", which creates "Switch.exe" and
  "Shim.exe". Copy these two executables into the same directory as
  "olsrd.exe" and you are ready to go.

Well, thanks for using an early release of a piece of software and
please bear with me if there are any problems. Please do also feel
free to suggest any features that you'd like to see in future
releases.

Thomas Lopatic <thomas@lopatic.de>, 2004-09-15

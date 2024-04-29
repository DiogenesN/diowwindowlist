# DioWWindowList
A Wayland application that shows a list of currently opened windows.
It was written without any GUI toolkits, using only libwayland.
Many things like: popup position, color, font, text size, highlight color were hardcoded,
since i wrote it primarily for myself, if you like it and you want some changes then
feel free to add any changes you want or email me.
On opening the application, you will see a small button on the top right edge of the screen.
Clicking on it, a popup window shows up with a list of all currently opened windows.
Clicking on any window title will activate (raise) the selected window.
You can also browse the windows by mouse wheel scrolling on the top right button.
It was tested on Debian 12 on Wayfire.

# What you can do with DioWWindowList
   1. Shows you a list of opened windows.
   2. Raise on click.
   3. Browse on mouse wheel scroll.

# Before building
	You need wayland-scanner to generate three header files:
	xdg-shell-client-protocol.h
	get the XML file from here:
	https://cgit.freedesktop.org/wayland/wayland-protocols/plain/stable/xdg-shell/xdg-shell.xml
	after downloading the file run this command to generate the glue code:
	wayland-scanner private-code < xdg-shell.xml > xdg-shell-protocol.c
	and this command to generate the header file:
	wayland-scanner client-header < xdg-shell.xml > xdg-shell-client-protocol.h

	wlr-layer-shell-unstable-v1-protocol.h
	get the XML from here:
	https://raw.githubusercontent.com/swaywm/wlroots/b7dc4f2990d1e6cdba38a7e9d2d286e48dd1a3eb/protocol/wlr-layer-shell-unstable-v1.xml
	after downloading the file run this command to generate the glue code:
	wayland-scanner private-code < wlr-layer-shell-unstable-v1.xml > wlr-layer-shell-unstable-v1.c
	and this command to generate the header file:
	wayland-scanner client-header < wlr-layer-shell-unstable-v1.xml > wlr-layer-shell-unstable-v1-protocol.h

	wlr-foreign-toplevel-management-unstable-v1-client-protocol.h
	get the XML file from here:
	https://raw.githubusercontent.com/swaywm/wlr-protocols/master/unstable/wlr-foreign-toplevel-management-unstable-v1.xml	after downloading the file run this command to generate the glue code:
	wayland-scanner private-code < wlr-foreign-toplevel-management-unstable-v1.xml > wlr-foreign-toplevel-management-unstable-v1.c
	and this command to generate the header file:
	wayland-scanner client-header < wlr-foreign-toplevel-management-unstable-v1.xml > wlr-foreign-toplevel-management-unstable-v1-client-protocol.h

	after generating all those *.c and *.h files, and installing all the required libs,
	move them to the src/ directory

	also you need to install the following libs:
		make
		pkgconf
		librsvg2-dev
		libcairo2-dev
		libwayland-dev

	on Debian run the following command:
		sudo apt install libwayland-dev libcairo2-dev librsvg2-dev make

	after going through all the above steps, go to the next step in this tutorial.

# Installation/Usage
  1. Open a terminal and run:

		 ./configure

  2. if all went well then run:

		 make
		 sudo make install
		 
		 (if you just want to test it then run: make run)
		
  3. Run the application:
  
		 diowwindowlist

# Screenshots
 
![Alt text](https://raw.githubusercontent.com/DiogenesN/diowwindowlist/main/diowwindowlist.png)

![Alt text](https://raw.githubusercontent.com/DiogenesN/diowwindowlist/main/diowwindowlist2.png)

That's it!

# Support

   My Libera IRC support channel: #linuxfriends
   Matrix: https://matrix.to/#/#linuxfriends2:matrix.org
   Email: nicolas.dio@protonmail.com

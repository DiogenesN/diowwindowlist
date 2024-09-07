# SPDX-License-Identifier: GPL-2.0-or-later

# DioWWindowList
A Wayland application that shows a list of currently opened windows. It was written without any GUI toolkits, using only libwayland. Many things like: popup position, color, font, text size, highlight color were hardcoded, since i wrote it primarily for myself, if you like it and you want some changes then feel free to add any changes you want or email me. On opening the application, you will see a small button on the top right edge of the screen. Clicking on it, a popup window shows up with a list of all currently opened windows.
Clicking on any window title will activate (raise) the selected window. You can also browse the windows by mouse wheel scrolling on the top right button. You can minimize a window if you click on the title twice in a row. Right click on the button, terminates the applicatoin.
It was tested on Debian 12 on Wayfire.

# What you can do with DioWWindowList
   1. Shows you a list of opened windows.
   2. Raise on click.
   3. Browse on mouse wheel scroll.
   4. Minimize the window if you click the same title twice in a row.

# Before building
install the following libs:

		make
		pkgconf
	 	librsvg2-dev
		libcairo2-dev
		libwayland-dev

on Debian run the following command:

		sudo apt install libwayland-dev libcairo2-dev librsvg2-dev cmake

# Installation/Usage
  1. Open a terminal and run:

    chmod +x ./configure
    ./configure

  2. if all went well then run:

    make
    sudo make install

    (if you just want to test it then run: make run)

  3. Run the application:

    diowwindowlist

# Configuration
The application creates the following configuration file

		~/.config/diowwindowlist/diowwindowlist.conf

   the content of the configuration file:

		icons_theme=none
		posx=-7
		posy=7
		cut_string_workaround=true

   to have support for icons, add the full path to your icon directory e.g.:

		icons_theme=/usr/share/icons/Lyra-blue-dark

   posx defines the x position of the popup window, on a screen resolution 1920x1080, set it to:

		posx=-1270

   posy defines the upper position of the popup window, on a screen resolution 1920x1080, set it to:

		posy=970

   cut_string_workaround is either true or false, it cuts the long multibyte strings, if it breaks the app then set it to false.

   any change in the configuration file requires application restart, right click on the button will close the application after that launch it again.

# Screenshots
 
![Alt text](https://raw.githubusercontent.com/DiogenesN/diowwindowlist/main/diowwindowlist.png)

![Alt text](https://raw.githubusercontent.com/DiogenesN/diowwindowlist/main/diowwindowlist2.png)

That's it!

# Support

   My Libera IRC support channel: #linuxfriends

   Matrix: https://matrix.to/#/#linuxfriends2:matrix.org

   Email: nicolas.dio@protonmail.com

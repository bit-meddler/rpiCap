# Setting up for RpiCap dev on a fesh Raspbian image
Because I keep bricking my pi trying to write this stuff.

1. Image and Install
2. Let piwiz complete - this is glacially slow :(
3. Enable SSH
    - `sudo raspi-config` > interface > SSH > Yes
4. Install TightVNC
    - `sudo apt install tightvncserver`
    - `tightvncserver -geometry 1280x720` to 1st launch and set password
5. Make the code repo
    - `sudo mkdir \code`
    - `sudo chmod 0777 \code`
    - `cd \code`
6. Clone the repos
    - `git clone https://github.com/bit-meddler/Gimli.git`
    - `git clone https://github.com/bit-meddler/rpiCap.git`
7. Install Python dependancies
    - `pip3 install -r \code\Gimli\requirements.txt`
    - Currently we can't pip install PySide2, if pip fails you need to manually install all the deps, for Qt apt-get it:
      - `sudo apt-get install --upgrade python3-pyside2.QtCore python3-pyside2.QtGui python3-pyside2.QtWidgets python3-pyside2.QtOpenGL`
      - It actually doesn't work anyway, my shaders target a very reasonable Core 330 but the pi can't even handle that.
8. Install C++ dev tools
    - sdl2 `apt-get install libsdl2-dev`
    - boost python?
    - numpy dev?
9. Setup Genie with:
    - Nicer syntax highlighting
    - The Build/Compile string from "complie_notes.txt"

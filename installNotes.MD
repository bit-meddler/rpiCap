# Setting up for RpiCap dev on a fesh Raspbian image
Because I keep bricking my pi trying to write this stuff.

1. Image and Install
2. Let piwiz complete - this is glacially slow :(
3. Enable SSH
  - `sudo raspi-config` > interface > SSH
4. Install TightVNC
  - `sudo apt update`
  - `sudo apt install tightvncserver`
  - `tightvncserver -geometry 1280x720` to launch and set password
5. Make the code repo
  - `sudo mkdir \code`
  - `sudo chmod 0777 \code`
  - `cd \code`
6. Clone the repos
  - `git clone https://github.com/bit-meddler/Gimli.git`
  - `git clone https://github.com/bit-meddler/rpiCap.git`
7. Install Python dependancies
  - `pip3 install -r \code\Gimli\requirements.txt`
  - This will be slow as PyZMQ builds from source
8. Install C++ dev tools
  - Can't remember this for the life of me :(
  - gcc
  - boost python
  - sdl2
  - numpy dev?

# Setting up for RpiCap dev on a fesh Raspbian image
Because I keep bricking my pi trying to write this stuff.

1. Image and Install
2. Let piwiz complete - this is glacially slow :(
3. Make the code repo
    `sudo mkdir \code`
    `sudo chmod 0777 \code`
    `cd \code`
4. Clone the repos
    `git clone https://github.com/bit-meddler/Gimli.git`
    `git clone https://github.com/bit-meddler/rpiCap.git`
5. Install Python dependancies
    `pip3 install -r \code\Gimli\requirements.txt`
    This will be slow as PyZMQ builds from source
6. Install C++ dev tools

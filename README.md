# dive
POV Display Driver for BeagleBone (Angstrom Linux)

Background and Documentation of the project can be found at:
   *  http://furiousgreencloud.com/pov
    
This is is a C code that draw the ROWs and SLICEs of a 3D Persistence of Vision
Display to make up a static VOLUME

The VOLUME is then animated by sending new VOLUMES to the display.

The Code neede the following Library:

   * Beagle Bone IO Library (https://github.com/furiousgreencloud/beagleboneIO)
  
Which must be build with "Raw Memeory Mapped IO" for greater speed.

i.e. in beagleboneIO/src:
    ./configure -enable-gpiomem=yes
    make install

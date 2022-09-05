### oai - Simple terminal image viewer ###
###### Still under development ######

TODO List

- [x] Fix color corrections for different image types
- [x] Keep image aspect ratio
- [x] Set image centered as default
- [ ] Support more image formats (tiff, gif, etc)
- [ ] Re-draw on zoom-in/zoom-out


IDEAS
- [ ] Implement ffmpeg/ffplay/mplayer/mpv to play videos in terminal (seperate project?)
- [ ] In-terminal opengl rendering surface (seperate project?)



##### Build (debian/ubuntu based) #####
````
git clone https://github.com/olealgoritme/oai
cd oai
./install_deps.sh
make
````

##### Install oai system wide #####
````
sudo make install
````



##### Use in terminal emulator #####
- arrow keys moves image inside terminal 
- +/- for zoom in/out (still buggy, needs fix)
````
oai stepsis.png
oai stepsis.png -s 0.3 (downscale to 30%)
oai stepsis.png -s 2.0 (upscale to 200%)
````

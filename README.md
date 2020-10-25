### oai - Simple terminal image viewer ###
###### Still under development ######

TODO List

- [x] Fix color corrections for different image types
- [ ] Support more image formats (tiff, gif, etc)


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
````
oai cool_picture.jpg
oai cool_picture.jpg -s 0.3 (scale to 30%)
oai cool_picture.jpg -s 2.0 (scale to 200%) --centered
````

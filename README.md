#### oaimg - terminal image viewer ####

- Still under development

TODO:
    Fix color corrections for different image types



Build (debian/ubuntu based)
````
git clone https://github.com/olealgoritme/oaimg
cd oaimg
./install_deps.sh
make
````

Install oaimg system wide
````
sudo make install
````



##### Use in terminal emulator #####
````
oaimg cool_picture.jpg
oaimg cool_picture.jpg -s 0.3 (scale to 30%)
oaimg cool_picture.jpg -s 2.0 (scale to 200%) --centered
````

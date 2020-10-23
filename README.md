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



##### Usage #####
````
oaimg /home/user/Pictures/cool_picture.jpg
oaimg /home/user/Pictures/cool_picture.jpg -s 0.3 (scale to 30%)
oaimg /home/user/Pictures/cool_picture.jpg -s 0.3 (scale to 30%) --centered (centered)
````

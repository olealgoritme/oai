#!/bin/sh
# dependencies for oai

# deb / ubuntu
apt install \
	libcairo2 \
	libcairo2-dev \
	libxcb1 \
	libxcb1-dev \
	libxcb-xkb1 \
	libxcb-xkb-dev

# arch
pacman -S \
	cairo \
	cairo-devel \
	libxcb \
	xcb-proto \
	libxkbcommon \
	libxkbcommon-x11

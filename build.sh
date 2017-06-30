#!/usr/bin/env bash
g++ -o aniniscale -g -Wall main.cpp `pkg-config vips-cpp --cflags --libs` -std=c++11

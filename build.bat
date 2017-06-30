g++ -o aniniscale -g -Wall main.cpp ^
-lvips-cpp -lvips -lgobject-2.0 -lglib-2.0 -lintl -std=c++11 ^
-static-libgcc -static-libstdc++ ^
-L%VIPS_PATH%\bin ^
-L%VIPS_PATH%\lib ^
-I%VIPS_PATH%\include ^
-I%VIPS_PATH%\include\glib-2.0 ^
-I%VIPS_PATH%\lib\glib-2.0\include

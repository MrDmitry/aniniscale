set VIPS_FLAGS=-static-libgcc -static-libstdc++ -lvips-cpp -lvips -lgobject-2.0 -lglib-2.0 -lintl -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic

set CPP_EXTRA_FLAGS=-L%VIPS_PATH%\bin ^
-L%VIPS_PATH%\lib ^
-I%VIPS_PATH%\include ^
-I%VIPS_PATH%\include\glib-2.0 ^
-I%VIPS_PATH%\lib\glib-2.0\include

mkdir .obj 2> NUL

make CPP_EXTRA_FLAGS="%CPP_EXTRA_FLAGS%" VIPS_FLAGS="%VIPS_FLAGS%"

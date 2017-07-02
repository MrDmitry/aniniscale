aniniscale v1.0.0
Depends on [libvips](https://github.com/jcupitt/libvips) for image processing

Downscales image by reducing blocks in original image to a single pixel of dominant color.

Divides given image into multiple task areas. The size of each area is determined by a number of factors:
 - initial image size;
 - number of processing threads;
 - block size;
 - number of blocks inside each task.

Each task consists of the following steps:
1. Pick a block of pixels
2. Find dominant color in this block
3. Write dominant color to resulting image
After all tasks are complete, resulting image is saved as png

Usage:
```
aniniscale [options] -i/--input INPUT -o/--output OUTPUT

Required arguments:
    -i INPUT, --input=INPUT         path to input image
    -o OUTPUT, --output=OUTPUT      path to output image

Optional arguments:
    -h, --help                      prints detailed help message
    -x NUM, --x-block=NUM           block size on X axis [default 8]
    -y NUM, --y-block=NUM           block size on Y axis [default 8]
    -t NUM, --task-block-side=NUM   maximum number of blocks in any processing task [default 64]
    -r NUM, --reporting-timeout=NUM minimum timeout between log reports in seconds [default 5]
```

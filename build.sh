#!/usr/bin/env bash
VIPS_FLAGS=`pkg-config vips-cpp --libs`
CPP_EXTRA_FLAGS=`pkg-config vips-cpp --cflags`

make VIPS_FLAGS="${VIPS_FLAGS}" CPP_EXTRA_FLAGS="${CPP_EXTRA_FLAGS}" -j$(nproc)

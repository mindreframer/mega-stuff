#!/bin/sh

export GI_TYPELIB_PATH=../mega
export LD_LIBRARY_PATH=../mega/.libs/

gjs -I . test-api.js

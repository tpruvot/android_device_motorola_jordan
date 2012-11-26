#!/bin/bash

[ -z "$ANDROID_BUILD_TOP"] && ANDROID_BUILD_TOP=/cm9

$ANDROID_BUILD_TOP/out/host/linux-x86/bin/mkbootfs root/ | gzip > ramdisk.gz


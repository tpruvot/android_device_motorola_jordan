#!/bin/bash

gunzip ramdisk.gz

rm -rf ./root
mkdir root
cd root && cat ../ramdisk | cpio -i


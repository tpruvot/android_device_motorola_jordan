#!/system/xbin/ash
#
# Load PVR Driver.

export PATH=/system/xbin:/system/bin:/sbin

insmod /system/lib/modules/symsearch.ko 2>/dev/null # must be already loaded

echo 0 > /sys/devices/platform/omapdss/overlay0/enabled
echo 4194304 > /sys/devices/platform/omapfb/graphics/fb0/size
echo 1 > /sys/devices/platform/omapdss/overlay0/enabled

# Load the new SGX modules
insmod /system/lib/modules/pvrsrvkm-2ndboot.ko
insmod /system/lib/modules/omaplfb-2ndboot.ko


#!/system/xbin/sh
#
# Sample script made to switch usb mode from the command line

PATH=/system/xbin:/system/bin:/sbin:/data/local/bin

# are we currently on RNDIS mode?
IS_RNDIS=$(cat /data/usbd/current_state 2>/dev/null | grep rndis -c)

# if we're in RNDIS mode and we're switching to a non RNDIS mode
if [ "$IS_RNDIS" = "1" ] ; then
    if [ ! "$1" = "3" ] ; then
        logwrapper /system/bin/am broadcast -a com.motorola.intent.action.USB_TETHERING_TOGGLED --ei state 0
    fi
fi

# if we're switching to RNDIS mode
if [ "$1" = "3" ] ; then
    logwrapper /system/bin/am broadcast -a com.motorola.intent.action.USB_TETHERING_TOGGLED --ei state 1
else
    logwrapper /system/bin/am broadcast -a com.motorola.intent.action.USB_MODE_SWITCH_FROM_UI --ei USB_MODE_INDEX $1
fi


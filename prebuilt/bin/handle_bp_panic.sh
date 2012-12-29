#!/system/bin/sh

# stop panic_daemon restarting
setprop ctl.stop panic_daemon

# let UI handle the failure
am startservice -n com.cyanogenmod.defyparts/.BpPanicHandlerService 2>&1 | grep Error
if [ $? -eq 0 ]; then
    # some error occured, use fallback and reboot directly
    reboot bppanic
fi

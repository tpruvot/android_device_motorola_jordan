#!/sbin/sh

######## BootMenu Script
######## Format to ext4

source /system/bootmenu/script/_config.sh

######## Main Script
#Unmount
umount /data
umount /cache

#Scan on errors
/system/bin/e2fsck -y $PART_DATA
/system/bin/e2fsck -y $PART_CACHE

#Format to ext4
/system/bin/make_ext4fs $PART_DATA
/system/bin/make_ext4fs $PART_CACHE

#journal
/system/bin/tune2fs -o journal_data_writeback $PART_DATA
/system/bin/tune2fs -O ^has_journal $PART_DATA
/system/bin/tune2fs -o journal_data_writeback $PART_CACHE
/system/bin/tune2fs -O ^has_journal $PART_CACHE

#Mount cache
mount -t ext4 -o nosuid,nodev,noatime,nodiratime,barrier=1 $PART_CACHE /cache

exit

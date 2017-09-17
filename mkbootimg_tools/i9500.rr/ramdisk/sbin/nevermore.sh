#!/system/bin/sh

BB=/sbin/busybox;

# Remount root and system read/write
mount -t rootfs -o remount,rw rootfs
mount -o remount,rw /system
mount -o remount,rw /data

#
# Setup for Cron Task
#
if [ ! -d /data/.nvm ]; then
	$BB mkdir -p /data/.nvm
	$BB chmod -R 0777 /.nvm/
fi;

# Copy Cron files
$BB cp -a /res/crontab/ /data/
if [ ! -e /data/crontab/custom_jobs ]; then
	$BB touch /data/crontab/custom_jobs;
	$BB chmod 777 /data/crontab/custom_jobs;
fi;

#
# Stop Google Service and restart it on boot (dorimanx)
# This removes high CPU load and ram leak!
#
if [ "$($BB pidof com.google.android.gms | wc -l)" -eq "1" ]; then
	$BB kill $($BB pidof com.google.android.gms);
fi;
if [ "$($BB pidof com.google.android.gms.unstable | wc -l)" -eq "1" ]; then
	$BB kill $($BB pidof com.google.android.gms.unstable);
fi;
if [ "$($BB pidof com.google.android.gms.persistent | wc -l)" -eq "1" ]; then
	$BB kill $($BB pidof com.google.android.gms.persistent);
fi;
if [ "$($BB pidof com.google.android.gms.wearable | wc -l)" -eq "1" ]; then
	$BB kill $($BB pidof com.google.android.gms.wearable);
fi;

# Check for init.d folder and create it if it doesn't available
if [ ! -e /system/etc/init.d ] ; then
	mkdir /system/etc/init.d
	chown -R root.root /system/etc/init.d
	chmod -R 755 /system/etc/init.d
else
	chown -R root.root /system/etc/init.d
	chmod -R 755 /system/etc/init.d
fi

# Run init.d scripts
export PATH=${PATH}:/system/bin:/system/xbin
$BB run-parts /system/etc/init.d

chmod 777 /sbin/uci;
chmod 777 /res/synapse/*;
chmod 777 /res/synapse/actions/*;
/sbin/uci

mount -o remount,rw -t auto /system;
chmod -R 777 /system/etc/init.d;
mount -o remount,ro -t auto /system;

sync
mount -t rootfs -o remount,ro rootfs
mount -o remount,ro /system

# Nevermore tweaks
echo "0x0FA4 0x0404 0x0170 0x1DB9 0xF233 0x040B 0x08B6 0x1977 0xF45E 0x040A 0x114C 0x0B43 0xF7FA 0x040A 0x1F97 0xF41A 0x0400 0x1068" > /sys/class/misc/wolfson_control/eq_sp_freqs;
echo -11 > /sys/class/misc/wolfson_control/eq_sp_gain_1;
echo -7 > /sys/class/misc/wolfson_control/eq_sp_gain_2;
echo 0 > /sys/class/misc/wolfson_control/eq_sp_gain_3;
echo 0 > /sys/class/misc/wolfson_control/eq_sp_gain_4;
echo -0 > /sys/class/misc/wolfson_control/eq_sp_gain_5;
echo 1 > /sys/class/misc/wolfson_control/switch_sp_privacy;
echo 9 > /sys/class/misc/wolfson_control/eq_hp_gain_1;
echo 10 > /sys/class/misc/wolfson_control/eq_hp_gain_2;
echo 6 > /sys/class/misc/wolfson_control/eq_hp_gain_3;
echo -4 > /sys/class/misc/wolfson_control/eq_hp_gain_4;
echo 2 > /sys/class/misc/wolfson_control/eq_hp_gain_5;
echo 1 > /sys/class/lcd/panel/power_reduce;
echo 18 > /sys/class/misc/mdnie/hook_control/cs_red;
echo 10 > /sys/class/misc/mdnie/hook_control/cs_yellow;
echo 11 > /sys/class/misc/mdnie/hook_control/cs_green;
echo 15 > /sys/class/misc/mdnie/hook_control/cs_blue;
echo 9 > /sys/class/misc/mdnie/hook_control/cs_cyan;
echo 10 > /sys/class/misc/mdnie/hook_control/cs_magenta;
echo 4 > /sys/class/misc/mdnie/black_increase_value;
echo 17 > /sys/class/misc/mdnie/hook_control/cs_weight;
echo 3 > /sys/class/misc/mdnie/black_increase_value;
for i in /sys/block/*/queue/add_random;do echo 0 > $i;done
echo "0" > /proc/sys/kernel/nmi_watchdog;
echo 0 > /sys/module/fimg2d_drv/parameters/g2d_debug;
echo 50 > /sys/class/devfreq/exynos5-busfreq-mif/polling_interval;
echo 70 > /sys/class/devfreq/exynos5-busfreq-mif/time_in_state/upthreshold;
echo 0 > /sys/devices/virtual/sensors/proximity_sensor/prox_cal;
echo 1 > /sys/devices/virtual/sensors/proximity_sensor/prox_cal;
echo 0 > /sys/module/pvrsrvkm/parameters/gPVRDebugLevel;
echo 0 > /sys/module/pvrsrvkm/parameters/gPVREnableVSync;
echo 0 > /proc/sys/kernel/randomize_va_space;
echo "4096" > /proc/sys/vm/min_free_kbytes;
echo "0" > /proc/sys/vm/oom_kill_allocating_task;
echo "0" > /proc/sys/vm/panic_on_oom;
echo "30" > /proc/sys/kernel/panic;
echo "0" > /proc/sys/kernel/panic_on_oops;
echo "0" > /proc/sys/vm/laptop_mode;
echo "130" > /proc/sys/vm/swappiness;
echo "10" > /proc/sys/vm/vfs_cache_pressure;
echo "10" > /proc/sys/vm/dirty_ratio;
echo "5" > /proc/sys/vm/dirty_background_ratio;
echo "500" > /proc/sys/vm/dirty_expire_centisecs;
echo "800" > /proc/sys/vm/dirty_writeback_centisecs;
echo "3" > /proc/sys/vm/drop_caches;
chmod 444 /dev/erandom;
chmod 444 /dev/frandom;

# Block Mdnie Hook

echo "Mdnie Executed" > /data/mdniehookblocked
touch /data/mdniehookblocked

# Block Hook Tweaks

echo 0 > /sys/class/misc/mdnie/hook_control/s_edge_enhancement
echo 1 > /sys/class/misc/mdnie/hook_control/s_edge_enhancement
chmod 777 /sys/class/misc/mdnie/hook_control/scr_black_blue
echo 0 > /sys/class/misc/mdnie/hook_control/scr_black_blue
chmod 444 /sys/class/misc/mdnie/hook_control/scr_black_blue
chmod 777 /sys/class/misc/mdnie/hook_control/scr_black_green
echo 0 > /sys/class/misc/mdnie/hook_control/scr_black_green
chmod 444 /sys/class/misc/mdnie/hook_control/scr_black_green
chmod 777 /sys/class/misc/mdnie/hook_control/scr_black_red
echo 0 > /sys/class/misc/mdnie/hook_control/scr_black_red
chmod 444 /sys/class/misc/mdnie/hook_control/scr_black_red
chmod 777 /sys/class/misc/mdnie/hook_control/scr_blue_blue
echo 255 > /sys/class/misc/mdnie/hook_control/scr_blue_blue
chmod 444 /sys/class/misc/mdnie/hook_control/scr_blue_blue
chmod 777 /sys/class/misc/mdnie/hook_control/scr_blue_green
echo 0 > /sys/class/misc/mdnie/hook_control/scr_blue_green
chmod 444 /sys/class/misc/mdnie/hook_control/scr_blue_green
chmod 777 /sys/class/misc/mdnie/hook_control/scr_blue_red
echo 0 > /sys/class/misc/mdnie/hook_control/scr_blue_red
chmod 444 /sys/class/misc/mdnie/hook_control/scr_blue_red
chmod 777 /sys/class/misc/mdnie/hook_control/scr_cyan_blue
echo 255 > /sys/class/misc/mdnie/hook_control/scr_cyan_blue
chmod 444 /sys/class/misc/mdnie/hook_control/scr_cyan_blue
chmod 777 /sys/class/misc/mdnie/hook_control/scr_cyan_green
echo 240 > /sys/class/misc/mdnie/hook_control/scr_cyan_green
chmod 444 /sys/class/misc/mdnie/hook_control/scr_cyan_green
chmod 777 /sys/class/misc/mdnie/hook_control/scr_cyan_red
echo 42 > /sys/class/misc/mdnie/hook_control/scr_cyan_red
chmod 444 /sys/class/misc/mdnie/hook_control/scr_cyan_red
chmod 777 /sys/class/misc/mdnie/hook_control/scr_green_blue
echo 0 > /sys/class/misc/mdnie/hook_control/scr_green_blue
chmod 444 /sys/class/misc/mdnie/hook_control/scr_green_blue
chmod 777 /sys/class/misc/mdnie/hook_control/scr_green_green
echo 245 > /sys/class/misc/mdnie/hook_control/scr_green_green
chmod 444 /sys/class/misc/mdnie/hook_control/scr_green_green
chmod 777 /sys/class/misc/mdnie/hook_control/scr_green_red
echo 64 > /sys/class/misc/mdnie/hook_control/scr_green_red
chmod 444 /sys/class/misc/mdnie/hook_control/scr_green_red
chmod 777 /sys/class/misc/mdnie/hook_control/scr_magenta_blue
echo 255 > /sys/class/misc/mdnie/hook_control/scr_magenta_blue
chmod 444 /sys/class/misc/mdnie/hook_control/scr_magenta_blue
chmod 777 /sys/class/misc/mdnie/hook_control/scr_magenta_green
echo 20 > /sys/class/misc/mdnie/hook_control/scr_magenta_green
chmod 444 /sys/class/misc/mdnie/hook_control/scr_magenta_green
chmod 777 /sys/class/misc/mdnie/hook_control/scr_magenta_red
echo 255 > /sys/class/misc/mdnie/hook_control/scr_magenta_red
chmod 444 /sys/class/misc/mdnie/hook_control/scr_magenta_red
chmod 777 /sys/class/misc/mdnie/hook_control/scr_red_blue
echo 0 > /sys/class/misc/mdnie/hook_control/scr_red_blue
chmod 444 /sys/class/misc/mdnie/hook_control/scr_red_blue
chmod 777 /sys/class/misc/mdnie/hook_control/scr_red_green
echo 17 > /sys/class/misc/mdnie/hook_control/scr_red_green
chmod 444 /sys/class/misc/mdnie/hook_control/scr_red_green
chmod 777 /sys/class/misc/mdnie/hook_control/scr_red_red
echo 247 > /sys/class/misc/mdnie/hook_control/scr_red_red
chmod 444 /sys/class/misc/mdnie/hook_control/scr_red_red
chmod 777 /sys/class/misc/mdnie/hook_control/scr_white_blue
echo 246 > /sys/class/misc/mdnie/hook_control/scr_white_blue
chmod 444 /sys/class/misc/mdnie/hook_control/scr_white_blue
chmod 777 /sys/class/misc/mdnie/hook_control/scr_white_green
echo 245 > /sys/class/misc/mdnie/hook_control/scr_white_green
chmod 444 /sys/class/misc/mdnie/hook_control/scr_white_green
chmod 777 /sys/class/misc/mdnie/hook_control/scr_white_red
echo 255 > /sys/class/misc/mdnie/hook_control/scr_white_red
chmod 444 /sys/class/misc/mdnie/hook_control/scr_white_red
chmod 777 /sys/class/misc/mdnie/hook_control/scr_yellow_blue
echo 8 > /sys/class/misc/mdnie/hook_control/scr_yellow_blue
chmod 444 /sys/class/misc/mdnie/hook_control/scr_yellow_blue
chmod 777 /sys/class/misc/mdnie/hook_control/scr_yellow_green
echo 241 > /sys/class/misc/mdnie/hook_control/scr_yellow_green
chmod 444 /sys/class/misc/mdnie/hook_control/scr_yellow_green
chmod 777 /sys/class/misc/mdnie/hook_control/scr_yellow_red
echo 255 > /sys/class/misc/mdnie/hook_control/scr_yellow_red
chmod 444 /sys/class/misc/mdnie/hook_control/scr_yellow_red

#
# Set correct r/w permissions for LMK parameters
#

chmod 666 /sys/module/lowmemorykiller/parameters/cost;
chmod 666 /sys/module/lowmemorykiller/parameters/adj;
chmod 666 /sys/module/lowmemorykiller/parameters/minfree;

#
# We need faster I/O so do not try to force moving to other CPU cores (dorimanx)
#
for i in /sys/block/*/queue; do
        echo "2" > $i/rq_affinity
done

#
# Allow untrusted apps to read from debugfs (mitigate SELinux denials)
#
/system/xbin/supolicy --live \
	"allow untrusted_app debugfs file { open read getattr }" \
	"allow untrusted_app sysfs_lowmemorykiller file { open read getattr }" \
	"allow untrusted_app sysfs_devices_system_iosched file { open read getattr }" \
	"allow untrusted_app persist_file dir { open read getattr }" \
	"allow debuggerd gpu_device chr_file { open read getattr }" \
	"allow netd netd capability fsetid" \
	"allow netd { hostapd dnsmasq } process fork" \
	"allow { system_app shell } dalvikcache_data_file file write" \
	"allow { zygote mediaserver bootanim appdomain }  theme_data_file dir { search r_file_perms r_dir_perms }" \
	"allow { zygote mediaserver bootanim appdomain }  theme_data_file file { r_file_perms r_dir_perms }" \
	"allow system_server { rootfs resourcecache_data_file } dir { open read write getattr add_name setattr create remove_name rmdir unlink link }" \
	"allow system_server resourcecache_data_file file { open read write getattr add_name setattr create remove_name unlink link }" \
	"allow system_server dex2oat_exec file rx_file_perms" \
	"allow mediaserver mediaserver_tmpfs file execute" \
	"allow drmserver theme_data_file file r_file_perms" \
	"allow zygote system_file file write" \
	"allow atfwd property_socket sock_file write" \
	"allow untrusted_app sysfs_display file { open read write getattr add_name setattr remove_name }" \
	"allow debuggerd app_data_file dir search" \
	"allow sensors diag_device chr_file { read write open ioctl }" \
	"allow sensors sensors capability net_raw" \
	"allow init kernel security setenforce" \
	"allow netmgrd netmgrd netlink_xfrm_socket nlmsg_write" \
	"allow netmgrd netmgrd socket { read write open ioctl }"

# Google play services wakelock fix
sleep 40
su -c "pm enable com.google.android.gms/.update.SystemUpdateActivity"
su -c "pm enable com.google.android.gms/.update.SystemUpdateService"
su -c "pm enable com.google.android.gms/.update.SystemUpdateService$ActiveReceiver"
su -c "pm enable com.google.android.gms/.update.SystemUpdateService$Receiver"
su -c "pm enable com.google.android.gms/.update.SystemUpdateService$SecretCodeReceiver"
su -c "pm enable com.google.android.gsf/.update.SystemUpdateActivity"
su -c "pm enable com.google.android.gsf/.update.SystemUpdatePanoActivity"
su -c "pm enable com.google.android.gsf/.update.SystemUpdateService"
su -c "pm enable com.google.android.gsf/.update.SystemUpdateService$Receiver"
su -c "pm enable com.google.android.gsf/.update.SystemUpdateService$SecretCodeReceiver"

cortexbrain_background_process=$(cat /res/synapse/nvm/cortexbrain_background_process);
if [ "$cortexbrain_background_process" == "1" ]; then
	sleep 30
	$BB nohup $BB sh /sbin/cortexbrain-tune.sh > /dev/null 2>&1 &
fi;

cron_master=$(cat /res/synapse/nvm/cron_master);
if [ "$cron_master" == "1" ];then
	$BB nohup $BB sh /res/crontab_service/service.sh 2> /dev/null;
fi;

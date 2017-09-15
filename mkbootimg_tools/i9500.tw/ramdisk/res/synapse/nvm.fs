#!/system/xbin/busybox sh

cat << CTAG
{
    name:File System,
    elements:[
    	{ SDescription:{
        	description:"\n\tFor now, this tab just displays the status of the three main partitions.\n\n",
        	background:0
        }},
    	{ SLiveLabel:{
		refresh:10000000,
		title:"Filesystem of /cache Partition",
		style:"normal",
		action:"
		if grep -q 'cache f2fs' /proc/mounts ; then
			echo F2FS;
		else
			echo EXT4;
		fi;"
	}},
	{ SSpacer:{
		height:1
	}},
	{ SLiveLabel:{
		refresh:10000000,
		title:"Filesystem of /data Partition",
		style:"normal",
		action:"
		if grep -q 'data f2fs' /proc/mounts ; then
			echo F2FS;
		else
			echo EXT4;
		fi;"
	}},
	{ SSpacer:{
		height:1
	}},
	{ SLiveLabel:{
		refresh:10000000,
		title:"Filesystem of /system Partition",
		style:"normal",
		action:"
		if grep -q 'system f2fs' /proc/mounts ; then
			echo F2FS;
		else
			echo EXT4;
		fi;"
	}},
	{ SSpacer:{
		height:1
	}},
	{ SPane:{
		title:"Filesystem Controls",
		description:""
        }},
	{ SButton:{
		label:"Remount /system as Writeable",
		action:"mount -o remount,rw \/system;
		echo Remounted \/system as Writable!;"
	}},
	{ SSpacer:{
		height:1
	}},
	{ SButton:{
		label:"Remount /system as Read-Only",
		action:"mount -o remount,ro \/system;
		echo Remounted \/system as Read-Only!;"
	}},
	{ SSpacer:{
		height:1
	}},
	{ SButton:{
		label:"Remount RootFS as Writeable",
		action:"/sbin/busybox mount -t rootfs -o remount,rw rootfs;
		echo Remounted RootFS as Writable!;"
	}},
	{ SSpacer:{
		height:1
	}},
	{ SButton:{
		label:"Remount RootFS as Read-Only",
		action:"/sbin/busybox mount -t rootfs -o remount,ro rootfs;
		echo Remounted RootFS as Read-Only!;"
	}},
	{ SSpacer:{
		height:2
	}},
    ]
}
CTAG

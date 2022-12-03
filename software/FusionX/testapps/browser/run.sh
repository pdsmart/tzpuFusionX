#!/bin/sh
### Please change "cn.ntp.org.cn" to local NTP server for products.
### Please disable ntpdata if target device would sync-time automatically.
BROWSER_PATH=/customer/browser
export FONTCONFIG_FILE=$BROWSER_PATH/fonts.conf
export USE_FFMPEG_DECODER=yes
export BROWN_MEMORY_LOW_LIMIT=0x10
export BROWN_DEBUG="player=2,bwpage=2,vdimp=2"
export CURL_CA_BUNDLE_PATH=$BROWSER_PATH/cacert.pem
export BROWN_ENABLE_TOUCHEVENT=YES
export BROWN_CURL_DISABLE_CA_CHECK=YES
#export BROWN_INIT_PANEL=YES
export BROWN_HOME=$BROWSER_PATH/BrownHome
export BROWN_RSRC=$BROWSER_PATH/BrownData
export BROWN_SCREEN_480P=YES
export BROWN_PAGE_SIZE=800x480
export BROWN_DFB_SIZE=800x480
#export BROWN_FFMPEG_CURL=YES
export BROWN_DFB_LAYER=-1
export BROWN_DFB_WINDOW=0
export LD_LIBRARY_PATH=$BROWSER_PATH/lib:$BROWSER_PATH/dep:$BROWSER_PATH/SSDlib:/customer/libdns:$LD_LIBRARY_PATH
#exec .$BROWSER_PATH/bin/bwnsvc -s -u "http://launcher.iserver.tv/sigmastar/index" --dfb:module-dir=$BROWSER_PATH/dep/directfb-1.2-9,tslib-devices=/dev/input/event0,mouse-source=none,no-sighandler,layer-bg-none,no-vt,linux-input-devices=/dev/input/event1 $@
exec $BROWSER_PATH/bin/bwnsvc -s -u file://$BROWSER_PATH/CusDemo/index.html --dfb:module-dir=$BROWSER_PATH/dep/directfb-1.2-9,tslib-devices=/dev/input/event0,mouse_source=none $@

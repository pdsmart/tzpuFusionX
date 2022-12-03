version：
########################## BROWN ############################
#
#     Brown Product :  BrownApp
#     Brown Version :  brown.79
#     Brown GitHash :  58beb2942976b3441b1211765fee0207267aa1bc
#     Customer Name :  IServer(Sigmastar/dev)
#     BWSDK GitHash :  4759a395929c32ecc8631016163a3830363a05a5
#
################################################################

修复问题：
1: 优化浏览器内核库，flash 占用 12M 减小至 7M
2：DFB 对接MI_GFX 作硬件加速，默认还是使用的是软件blit，要使用MI GFX Blit加速则只需要pkg_brown_ssd20x/dep/directfb-1.2-9/gfxdrivers/libdirectfb_ssd20x.sobk 重命名为libdirectfb_ssd20x.so
（底层MI_GFX硬件加速需要更新/SSD20X/Demo_Release/UI_DEMO/GFX 的patch，如果没有更新使用会绘制不出来。）
3：修复浏览器默认player 音量设置问题
4: 优化启动脚本dfb参数
5：修复一些播放bug


Todo：
1：UI跟视频旋转 的feature


当前默认启动方式为多进程启动：
启动步骤如下：
1：网络配置正常
2：MI sys 跟 panel的初始化并跑后台（panel初始化可以参考pkg_brown_ssd20x/DemoCode/dispinit.c 的代码，屏参需要配对）
3：./run.sh 启动浏览器后台进程，默认的url是"http://launcher.iserver.tv/sigmastar/index"
4：ipc 唤醒 浏览器前台，访问 "http://launcher.iserver.tv/sigmastar/index"网页 （ipc 唤醒可以参考pkg_brown_ssd20x/DemoCode/ipcdemo.cpp, 已经编译出的bin为ipcdemo 可直接执行）
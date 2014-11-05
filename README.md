## Camkit (Camera toolKit)
Camkit是一个摄像头相关的工具箱，使用C语言写成，包含了从：图像采集-->色彩转换-->H264编码-->RTP打包-->网络发送的全套接口。

可到[这里](ftp://ftp.andy87.com/camkit)下载已编译好的二进制版本。

### 编译

Camkit采用**cmake**构建系统，编译之前请确认已经安装了cmake。

遵循以下步骤完成编译和安装：

    ```shell
    cd Camkit_source_dir
    mkdir build
    cd build
    cmake ../ -Dkey=value
    make
    make install
    ```
    
其中**-Dkey=value**是可以配置的选项，支持的选项如下：

    1. DEBUG=ON|OFF，是否打开调试选项
    2. PLATFORM=FSL|RPI|OFF， 选择所使用的平台（Freescale, RaspberryPi或FFmpeg），具体见下文
    3. CC=path_to_c_compiler|OFF， 指定C编译器的路径
    4. ROOTSYS=path_to_root_sys|OFF， 指定查找库和头文件的根路径（配合3选项可以用来做交叉编译）

Camkit的视频采集采用标准的**V4L**接口，通常的USB摄像头均可以支持。

Camkit的色彩转换和H264编码支持三种平台，分别是是：

    1. FFmpeg (采用ffmpeg编码，依赖于avcodec.so和swscale.so)
    2. Raspberry Pi (采用OpenMax硬编码，依赖于ilclient.a， vcsm.so， bcm_host.so， openmaxil.so等库，由于色彩转换部分使用了ffmpeg，因此还依赖于其中的swscale库)
    3. Freescale I.MX6 (采用IPU和VPU硬编码，依赖于ipu.so和vpu.so)

#### PC平台编译安装

使用ffmpeg编码，Camkit可以在PC上使用，原则上应该是跨平台的，但由于作者的开发主要是在Linux上完成，其他平台未经测试，今后有时间再做移植。下面主要介绍如何在Linux上编译安装。

在Linux上编译安装非常简单，以Ubuntu为例，首先安装编译环境:

    ```
    sudo apt-get install cmake libavcodec54 libavcodec-dev libswscale2 libswscale-dev 
    ```

然后遵循上面的构建步骤，使用如下命令构建和编译：

    ```
    mkdir build
    cd build
    cmake ../
    make 
    make install
    ```
    
至此，`libcamkit.so`就已经安装到你的系统里了，默认应该在`/usr/local/lib`下，头文件在`/usr/local/include`下。`demo`中的程序默认不会安装，可在`build/demo`目录下找到并运行，参照`demo`程序就可开发自己的应用了。

#### 树莓派平台编译安装

要在树莓派上使用可以选择在PC上交叉编译，也可将源代码拷到树莓派上直接编译，这里介绍后一种方式。

首先用`scp`之类的工具将Camkit的源代码拷到树莓派上，进入源码目录。由于树莓派运行的也是Linux系统，原则上可以和PC上一样使用ffmpeg库，但是实际效果非常卡顿，每秒仅有1~2帧，cpu消耗90%左右，因此推荐使用针对树莓派的OpenMax方案，下面是编译说明。

使用OpenMax需要一些头文件和库，这些库一般都在`/opt/vc/`目录下，不需要另外安装，除了一个库例外：`libilclient.a`。这是一个简单的封装库，可以到[github](https://github.com/raspberrypi/firmware/tree/master/opt/vc/src/hello_pi/libs/ilclient)下载源代码，两个`.c`文件、一个`.h`文件和一个`Makefile`，将[上上层目录](https://github.com/raspberrypi/firmware/tree/master/opt/vc/src/hello_pi)下的`Makefile.include`文件也下载下来。将这些文件都拷到树莓派上，用以下命令安装：

    ``` 
    mkdir -p build/one/two
    cp ilclient.h ilclient.c ilcore.c Makefile build/one/two
    cp Makefile.include build
    cd build/one/two
    make
    cp libilclient.a /opt/vc/lib
    ```
    
最后一步将编译好的库拷贝到`/opt/vc/lib`目录下。

除了OpenMax库之外，Camkit做色彩转换是还是使用了ffmepg中的`libswscale`库，因此需要在树莓派上先安装，方式可PC上的相同，通过`apt-get`安装`libswscale`和`libswscale-dev`即可（如何是`Arch`平台使用`pacman -S ffmpeg`）。

一切都做完之后就可以编译Camkit了，进入Camkit源码目录，使用如下命令编译安装：

    ``` 
    mkdir build
    cd build
    cmake ../ -DPLATFORM=RPI
    make 
    make install
    ```
    
这样，Camkit就已经安装到你的树莓派上了，路径和PC上的相同，demo程序在`build/demo`目录下。

#### 飞思卡尔平台编译安装

待写

### 使用
Camkit的接口非常简单方便，每个子功能均遵循类似的接口。

    ```C
    xxxHandle = xxx_open(xxParams);     // 打开xxx handle，例如： capture_open, convert_open...
    ...                     // 具体操作
    xxx_close(xxxHandle);    // 关闭handle，例如capture_close, convert_close...
    ```

一般调用步骤如下：

    ```C
    struct cap_handle *caphandle = NULL;    // capture操作符
    struct cvt_handle *cvthandle = NULL;   // convert操作符
    struct enc_handle *enchandle = NULL;   // encode操作符
    struct pac_handle *pachandle = NULL;   // pack操作符
    struct net_handle *nethandle = NULL;   // network操作符
    
    struct cap_param capp;      // capture参数
    struct cvt_param cvtp;      // convert参数
    struct enc_param encp;      // encode参数
    struct pac_param pacp;      // pack参数
    struct net_param netp;      // network参数
    
    // 设置各项参数
    capp.xxx = xxx
    ...
    cvtp.xxx = xxx;
    ...
    encp.xxx = xxx;
    ...
    pacp.xxx = xxx;
    ...
    netp.xxx = xxx;
    ...
    
    // 使用设置好的参数打开各项功能
    caphandle = capture_open(capp);
    cvthandle = convert_open(cvtp);
    enchandle = encode_open(encp);
    pachandle = pack_open(pacp);
    nethandle = net_open(netp);
        
    capture_start(caphandle);       // 开始capture
    while(1)
    {
        capture_get_data(caphandle, ...);    // 获取一帧图像
        
        convert_do(cvthandle, ...);    // 转换,YUV422=>YUV420， 如果你的摄像头直接支持采集YUV420数据则不需要这一步
        
        while (encode_get_headers(enchandle, ...) == 1)     // 获取h264头，PPS/SPS
        {
        ...
        }
        
        encode_do(enchandle, ...);      // 编码一帧图像
        
        pack_put(pachandle, ...);   // 将编码后的图像送给打包器
        while(pack_get(pachandle, ...) == 1)    // 获取一个打包后的RTP包
        {
            net_send(nethandle, ...);   // 将RTP包发送出去
        }
    }
    capture_stop(caphandle);        // 停止capture
    
    // 关闭各项功能
    net_close(nethandle);
    pack_close(pachandle);
    encode_close(enchandle);
    convert_close(cvthandle);
    capture_close(caphandle);
    ```

Note: 

1. 其中的每一个子功能都可以独立使用，例如只做采集，或者图像编码之后写入文件而不做打包和发送等等。
2. 如果是使用官方的树莓派摄像头，则采集部分不可用，需要另外写代码，但后面的编码打包等功能均可正常使用。

PS: demo目录有两个完整的例子，可以参考之。

### 实例--在树莓派上运行demo查看实时录像
demo/camstream.c是一个演示实例，实现了实时发送视频流的功能。

假设我们要在树莓派上使用Camkit，树莓派和PC连在同一个路由器上。

首先，按照上面的讲解完成编译，在`build/demo`目录下可以看到一个`cktool`的可执行程序。

配置树莓派开启摄像头支持并分配`gpu_mem`，`Raspbian`系统通过`sudo raspi-config`，`Arch`系统参见[Wiki](https://wiki.archlinux.org/index.php/Raspberry_Pi)。

然后，在PC上用记事本打开`demo/video.sdp`文件，修改ip地址为PC的ip地址，假设为`192.168.1.2`，设置端口，假设为`8888`。运行VLC播放器，打开demo/video.sdp文件。

最后，在树莓派上运行：
    
    #cktool -s 15 -a 192.168.1.2 -p 8888
    
至此，应该就可以在PC端看到树莓派的实时视频了。

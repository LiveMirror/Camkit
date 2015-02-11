## Camkit (Camera toolKit)
Camkit是一个摄像头相关的工具箱，使用C语言写成，包含了从：图像采集-->色彩转换-->H264编码-->RTP打包-->网络发送的全套接口。

可到项目附件中下载已编译好的二进制版本。

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
    2. PLAT=FSL|RPI|PC， 选择所使用的平台（Freescale IMX, Raspberry Pi或PC），具体见下文
    3. CMAKE_TOOLCHAIN_FILE=cross_file，用于交叉编译，具体见下文

Camkit的视频采集采用标准的**V4L**接口，通常的USB摄像头均可以支持。

Camkit的色彩转换和H264编码支持三种平台，分别是是：

    1. PC Desktop(采用ffmpeg编码，依赖于ffmpeg中的libavutil、libavcodec和libswscale库)
    2. Raspberry Pi (采用GPU加速，依赖于vcos，vcsm， bcm_host， openmaxil等库)
    3. Freescale I.MX6 (采用IPU和VPU硬编码，依赖于ipu和vpu)

#### PC平台编译安装

使用ffmpeg编码，Camkit可以在PC上使用，原则上应该是跨平台的，但由于作者的开发主要是在Linux上完成，其他平台未经测试，今后有时间再做移植。下面主要介绍如何在Linux上编译安装。

在Linux上编译安装非常简单，以Ubuntu为例，首先安装编译环境:

    ```
    sudo apt-get install cmake libavcodec54 libavcodec-dev libswscale2 libswscale-dev libavutil52 libavutil-dev #库的版本号可有会有变化，请根据不同的系统做调整
    ```

然后遵循上面的构建步骤，使用如下命令构建和编译：

    ```
    mkdir build
    cd build
    cmake ../
    make 
    make install
    ```
    
安装完成将在你的电脑上创建3种文件：1. `cktool`工具；2. `libcamkit.so`库，3.开发头文件。程序的默认安装路径为`/usr/local`，可通过在构建时添加`-DCMAKE_INSTALL_PREFIX=where`选项指定其他路径。

#### 树莓派平台编译安装

要在树莓派上使用可以选择在PC上交叉编译，也可将源代码拷到树莓派上直接编译，这里介绍后一种方式。

首先用`scp`之类的工具将Camkit的源代码拷到树莓派上，进入源码目录。由于树莓派运行的也是Linux系统，原则上可以和PC上一样使用ffmpeg库，但是实际效果非常卡顿，每秒仅有1~2帧，cpu消耗90%左右，因此推荐使用针对树莓派的GPU加速方案，下面是编译说明。

使用GPU加速需要一些头文件和库，这些库一般都在`/opt/vc/`目录下，不需要另外安装。

编译过程非常简单，进入Camkit源码目录，使用如下命令编译安装：

    ``` 
    mkdir build
    cd build
    cmake ../ -DPLAT=RPI
    make 
    make install
    ```
    
这样，Camkit就已经安装到你的树莓派上了，路径和PC上的相同。

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
demo/cktool.c是运用Camkit的一个工具，实现了Camkit支持的全部功能。

使用方法：

    $cktool [options]

options：

1. -? 显示帮助信息
2. -d 是否显示调试信息，每一步操作都用一种符号打印表示。
3. -s 设置步骤 0/1/3/7/15 (0:只做采集, 1:采集+转换, 3:采集+转换+编码(默认), 7:采集＋转换+编码+打包, 15:采集+转换+编码+打包+发送)
4. -i 设置打开的摄像头设备(默认/dev/video0)
5. -o 设置写入的文件(配合-s选项可以写入各个阶段的数据，方便调试)
6. -a 设置网络端的ip地址
7. -p 设置网络端的端口号
8. -c 设置采集图像格式: 0: YUYV(默认), 1: YUV420
9. -w 设置视频宽 (640)
10. -h 设置视频高 (480)
11. -r 设置编码帧率 kbps (1000)
12. -f 设置帧率 (15)
13. -t 设置图像是否交织 (0)
14. -g 设置编码的gop大小 (12)

假设我们要在树莓派上使用Camkit，将树莓派和PC连在同一个路由器上。

    RPI(Camkit) <==> 路由器 <==> PC (VLC)

首先，按照上面的讲解完成编译、安装。

配置树莓派开启摄像头支持并分配`gpu_mem`，`Raspbian`系统通过`sudo raspi-config`，`Arch`系统参见[Wiki](https://wiki.archlinux.org/index.php/Raspberry_Pi)。

然后，在PC上用记事本打开`demo/video.sdp`文件，修改ip地址为PC的ip地址，假设为`192.168.1.2`，设置端口，假设为`8888`。运行VLC播放器，打开demo/video.sdp文件。

最后，在树莓派上运行：
 
    #cktool -s 15 -a 192.168.1.2 -p 8888 
    
至此，应该就可以在PC端的VLC窗口库看到树莓派的实时视频了。
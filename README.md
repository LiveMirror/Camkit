## Camkit (Camera toolKit)
Camkit是一个摄像头相关的工具箱，包含了从：图像采集-->色彩转换-->H264编码-->RTP打包-->网络发送的全套接口。

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

    1. **Raspberry Pi** (采用OpenMax硬编码，依赖于ilclient.a， vcsm.so， bcm_host.so， openmaxil.so等库，由于色彩转换部分使用了ffmpeg，因此还依赖于其中的swscale库)
    2. **Freescale I.MX6** (采用IPU和VPU硬编码，依赖于ipu.so和vpu.so)
    3. **FFmpeg** (采用ffmpeg编码，依赖于avcodec.so和swscale.so)

### 使用
Camkit的接口非常简单方便，每个子功能均遵循类似的接口。

    ```C
    xxxHandle = xxx_open(xxParams);     // 打开xxx handle，例如： capture_open, convert_open...
    ...                     // 具体操作
    xxx_close(xxxHandle);    // 关闭handle，例如capture_close, convert_close...
    ```

一般调用步骤如下：

    ```C
    struct cap_handle *caphandle;    // capture操作符
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
        
        convert_do(cvthandle, ...);    // 转换
        
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

Note: 其中的每一个子功能都可以独立使用，例如只做采集，或者图像编码之后写入文件而不做打包和发送等等。

PS: demo目录有两个完整的例子，可以参考之。

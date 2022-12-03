<img src='../images/tzpuFusionX_v1_0_layout.png' height='70%' width='70%' style="margin-left: auto; margin-right: auto; display: block;"/>

## <font style="color: yellow;" size="6">Foreword</font>

<div style="text-align: justify">
The tran<i>ZPU</i>ter<sup><i>FusionX</i></sup> is a spinoff concept from the tranZPUter series. It shares the same purpose, to replace the Z80 in a Sharp or similar system and provide it with features such as faster CPU, more memory, better graphics, quicker application loading etc. 
<div style="padding-top: 0.8em;"></div>

It shares similarities with the tran<i>ZPU</i>ter<sup><i>Fusion</i></sup> but instead of realising it in hardware via an FPGA it realises
it in software as an application. This is made possible by a SOM (System On a Module), not much bigger than an FPGA device yet provides an abundance of features, ie. dual-core 1.2GHz Cortex-A7 ARM CPU, 128MB RAM, Wifi, HD Video, SD Card, USB port and runs the Linux operating system.
<div style="padding-top: 0.8em;"></div>

Using the same base design as the tran<i>ZPU</i>ter<sup><i>Fusion</i></sup> it incorporates a CPLD to interface the Z80 host to the SOM and provide cycle accurate Z80 timing. The SOM interfaces to the CPLD via a 16bit bus to query signal status and initiate Z80 transactions.
<div style="padding-top: 0.8em;"></div>

A Linux Kernel driver instantiates a Z80 emulation which realises a Z80 CPU in software and in turn can command and control the host system. The kernel driver along with a controlling application
can provide a wealth of features to the host through this mechanism. The SOM is also connected to an SD Drive and USB 2.0 port so there is no limit to features which can be proivded.
<div style="padding-top: 0.8em;"></div>

Like the tran<i>ZPU</i>ter<sup><i>Fusion</i></sup>, the tran<i>ZPU</i>ter<sup><i>FusionX</i></sup> can provide enhanced video and sound to the host. The SOM incorporates dual DAC audio and a 2D GPU with configurable resolutions. 
<div style="padding-top: 0.8em;"></div>

To most retro users, the above wont have meaning or interest as it is primarily for software research and development. What the board can offer the end user is:
<ul style="line-height: 0.8em;"><font size="3">
  <li>Original host specifications<br>- there might be slight differences in the Z80 functionality as it is implemented in software but the Z80 hardware timing is accurate.</li>
  <li>Accelerator<br>- the Z80 can run at much higher speeds due to the abundance of memory and 1.2GHz dual-core processor, which would typically see performance equivalent to a 500MHz Z80.</li>
  <li>Emulation<br>- it is possible to emulate all the Sharp MZ series machines in software, experiencing it through the host system keyboard, monitor and I/O.</li>
  <li>Graphics<br>- all original Sharp MZ graphics modes, regardless of host, are available through GPU configuration and these can be selected and programmed via Basic etc.</li>
  <li>Sound<br>- the host will have access to the dual DAC channels, which can emulate the SN76489 or basic bit/timer sound of the Sharp series.</li>
  <li>Processors<br>- there are many software CPU implementations which can be ported to run on this platform, for example the BBC PiCoPro which uses the same Linux and ARM CPU platform. This in turn allows the potential for other machines, such as the BBC to run on this Sharp host.</li>
  <li>Linux<br>- using the host keyboard, speaker, monitor etc, a full blooded version of Linux can be utilised at the host console.</li>
</font></ul>


The tran<i>ZPU</i>ter<sup><i>FusionX</i></sup> is under development and this source and document repository will be updated as the project progresses. Some source such as the SigmaStar SDK
and documents will be added in due course.

</div>

--------------------------------------------------------------------------------------------------------

## <font style="color: yellow;" size="6">Daughter Boards</font>

<div style="text-align: justify">
The tranZPUter series was initially developed in the Sharp MZ-80A and was primarily a Z80 replacement. As the concept evolved and the tranZPUter SW-700 was developed for the MZ-700 it became more of an integral component of the machine, offering original and upgraded Video and Audio capabilites by intercepting and routing existing signals.
<div style="padding-top: 0.8em;"></div>

After significant developments on the tranZPUter SW-700 it became desirable to port it back to the MZ-80A and MZ-2000 but these machines had different CPU orientation and signal requirements, ie. driving internal and external monitor. This requirement led to the concept of daughter boards, where a specific board would be designed and developed for
the target host and would plug into the tranZPUter SW-700 card. Ideally I wanted to port the SW-700 to an MZ-800/MZ-1500 and X1 but the size of the card and orientation of the Z80 was a limitation.
<div style="padding-top: 0.8em;"></div>

During the design of the tran<i>ZPU</i>ter<sup><i>FusionX</i></sup> one of the main requirements was to make the board small and the Z80 orientation changeable and compatible with the tran<i>ZPU</i>ter<sup><i>Fusion</i></sup> so that it could fit many machines and be interchangeable. As the SW-700 also interfaced to the Video and Audio of the machines and each was quite different, it became apparent that the tran<i>ZPU</i>ter<sup>FusionX</sup> needed
to include a concept to allow different video/audio interfaces according to the targetted host. This concept was realised via daughter boards. Two connectors would link the tran<i>ZPU</i>ter<sup><i>FusionX</i></sup> to a daughter board which would be specifically designed for the intended host.
<div style="padding-top: 0.8em;"></div>

The daughter boards would be responsible for switching and mixing video/audio signals and to drive internal monitors and provide the correct input and output connectors for ease of installation.
<div style="padding-top: 0.8em;"></div>

Currently three daughter boards have been developed, for the MZ-700, MZ-80A and MZ-2000 and more will follow as the design progresses.
</div>


###### <font style="color: yellow;" size="5">MZ-700 Daughter Board</font>

<div style="text-align: justify">
The MZ-700 daughter board is designed to be inserted into mainboard modulator output and the modulator in turn connected to the daughter board. 
<div style="padding-top: 0.8em;"></div>

The MZ-700 requires switching of the original mainboard video and tran<i>ZPU</i>ter<sup><i>FusionX</i></sup> FPGA video to the existing output connectors. This is accomplished with analogue MUX switches.
<div style="padding-top: 0.8em;"></div>

The sound of the MZ-700 is sent directly to a speaker and in order to inject tran<i>ZPU</i>ter<sup><i>FusionX</i></sup> sound, the mainboard speaker output is routed to the daughter board, level converted and switched. The FPGA offers stereo sound so this is selectively switched/mixed with the original sound and fed to a Class D amplifier which then drives
the internal speaker.
<div style="padding-top: 0.8em;"></div>

This setup allows for emulated machines, whilst running in the host on the FPGA such as the MZ-800/MZ-1500, to output their sound to the internal speaker. An audio output connector is provided for connection to stereo speakers if desired.
</div>


<div style='content: ""; clear: both; display: table;'>
    <div style='width: 45%; padding: 5px; padding-left: 1em; float: left'>
        <img src='../images/VideoInterface_MZ700_v1_0_3D_Top.png' height='100%' width='100%' style="margin-left: auto; margin-right: auto; display: block;"/>
    </div>
    <div style='width: 50%; padding: 5px; padding-left: 1em; float: left'>
        <img src='../images/VideoInterface_MZ700_v1_0_3D_Bottom.png' height='100%' width='100%' style="margin-left: auto; margin-right: auto; display: block;"/>
    </div>
</div>

###### <font style="color: yellow;" size="5">MZ-2000 Daughter Board</font>

<div style="text-align: justify">
The MZ-2000 daughter board is designed to be inserted simultaneously into the mainboard monitor and IPL connectors. The daughter board switches video source to the internal monitor, either the original mainboard video or FPGA video. The video source for the internal monitor is independent to the external moonitor source so different images can
be displayed on the internal monitor and external source at the same time.
<div style="padding-top: 0.8em;"></div>

The external video output can be fed from the original mainboard video or the FPGA video, the external video uses its own framebuffer so can be different to the internal monitor.
<div style="padding-top: 0.8em;"></div>

The audio is intercepted and switched/mixed prior to being sent to the internal audio amplifier.
<div style="padding-top: 0.8em;"></div>

The daughter board presents all the original connectors so it is easily installed.
<div style="padding-top: 0.8em;"></div>

</div>

<div style='content: ""; clear: both; display: table;'>
    <div style='width: 50%; padding: 5px; padding-left: 1em; float: left'>
        <img src='../images/VideoInterface_MZ2000_v1_0_3D_Top.png' height='100%' width='100%' style="margin-left: auto; margin-right: auto; display: block;"/>
    </div>
    <div style='width: 50%; padding: 5px; padding-left: 1em; float: left'>
        <img src='../images/VideoInterface_MZ2000_v1_0_3D_Bottom.png' height='100%' width='100%' style="margin-left: auto; margin-right: auto; display: block;"/>
    </div>
</div>

###### <font style="color: yellow;" size="5">MZ-80A Daughter Board</font>

<div style="text-align: justify">
The MZ-80A daughter board is designed to be inserted into the original Hirose monitor connector with the tape recorder plug being inserted through the gap in the daughter board. The system RESET is routed through the daughter board via 2pin connectors.
<div style="padding-top: 0.8em;"></div>

The daughter board switches video source to the internal monitor, either the original mainboard video or FPGA video. The video source for the internal monitor is independent to the external moonitor source so different images can
be displayed on the internal monitor and external source at the same time.
<div style="padding-top: 0.8em;"></div>

The external video output can be fed from the original mainboard video or the FPGA video, the external video uses its own framebuffer so can be different to the internal monitor.
<div style="padding-top: 0.8em;"></div>

The audio is intercepted and switched/mixed prior to being sent to the internal audio amplifier.
<div style="padding-top: 0.8em;"></div>

The daughter board presents all the original connectors so it is easily installed.
</div>

<div style='content: ""; clear: both; display: table;'>
    <div style='width: 47%; padding: 5px; padding-left: 1em; float: left'>
        <img src='../images/VideoInterface_MZ80A_V1_0_3D_Top.png' height='100%' width='100%' style="margin-left: auto; margin-right: auto; display: block;"/>
    </div>
    <div style='width: 50%; padding: 5px; padding-left: 1em; float: left'>
        <img src='../images/VideoInterface_MZ80A_V1_0_3D_Bottom.png' height='100%' width='100%' style="margin-left: auto; margin-right: auto; display: block;"/>
    </div>
</div>
<div style="text-align: justify">


</div>

--------------------------------------------------------------------------------------------------------

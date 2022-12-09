<img src='http://eaw.app/images/FusionX_Wired.png' height='70%' width='70%' style="margin-left: auto; margin-right: auto; display: block;"/>

## <font style="color: yellow;" size="6">Overview</font>

<div style="text-align: justify; line-height: 1.4em; font-size: 0.8em; font-family:sans-serif;">
The tran<i>ZPU</i>ter<sup><i>FusionX</i></sup> is a spinoff concept from the tranZPUter series. It shares the same purpose, to replace the physical Z80 in a Sharp or similar system and provide it with features such as faster CPU, more memory, virtual devices, 
rapid application loading from SD card and by using a daughter board, better graphics and sound.
<div style="padding-top: 0.8em;"></div>

The <i>FusionX</i> board can also be used to power alternative CPU's on the host for testing, development and to provide a completely different software platform and applications. It can use the underlying hosts keyboard, monitor, I/O and additionally better graphics and sound provided by the <i>FusionX</i>.
<div style="padding-top: 0.8em;"></div>

It shares similarities with the tran<i>ZPU</i>ter<sup><i>Fusion</i></sup> but instead of realising it in hardware via an FPGA it realises
it in software as an application. This is made possible by a SOM (System On a Module), not much bigger than an FPGA device yet provides an abundance of features, ie. dual-core 1.2GHz Cortex-A7 ARM CPU, 128MB RAM, 256MB FlashRAM, Wifi, HD Video, SD Card, USB port and runs the Linux operating system.
<div style="padding-top: 0.8em;"></div>

Using the same base design as the tran<i>ZPU</i>ter<sup><i>Fusion</i></sup> it incorporates a CPLD to interface the Z80 host to the SOM and provide cycle accurate Z80 timing. The SOM interfaces to the CPLD via a 72MHz SPI channel and an 8bit bus to query signal status and initiate Z80 transactions.
<div style="padding-top: 0.8em;"></div>

In Z80 configuration, a Linux Kernel driver instantiates a Z80 emulation which realises a Z80 CPU in software which in turn can command and control the host system. The kernel driver along with a controlling application
can provide a wealth of features to the host through this mechanism. The SOM is also connected to an SD Drive and USB 2.0 port so there is no limit to features which can be proivded.
<div style="padding-top: 0.8em;"></div>

Like the tran<i>ZPU</i>ter<sup><i>Fusion</i></sup>, the tran<i>ZPU</i>ter<sup><i>FusionX</i></sup> can provide enhanced video and sound to the host. The SOM incorporates dual DAC audio and a 2D GPU with configurable resolutions switched onto the hosts video and audio outputs under software control.
<div style="padding-top: 0.8em;"></div>

The <i>FusionX</i> board is ideal for any developer wanting to physically program and interact with retro hardware using a Linux platform with Wifi and USB/Serial port connectivity.
<div style="padding-top: 0.8em;"></div>

To most retro users, in the early stages of <i>FusionX</i> development, the board wont have much use. As the project matures, a board can be obtained and installed into the Z80 socket of their Sharp or simlar Z80 based system (providing there is sufficient room to accommodate this board) and utilise the upgraded featues, such as:
<ul style="line-height: 0.9em;"><font size="3">
  <li><font style="color: orange;" size="3">Original host specifications</font><br>- the machine behaves just as though it had a physical Z80 within. There might be slight differences in the Z80 functionality as it is implemented in software but the Z80 hardware timing is accurate.</li>
  <li><font style="color: orange;" size="3">Accelerator</font><br>- the Z80 can run at much higher speeds due to the abundance of memory and 1.2GHz dual-core processor, which would typically see performance upto that of a 500MHz Z80.</li>
  <li><font style="color: orange;" size="3">Emulation</font><br>- emulation of all the Sharp MZ series machines, experiencing it through the host system keyboard, monitor and I/O.</li>
  <li><font style="color: orange;" size="3">Graphics</font><br>- all original Sharp MZ graphics modes, regardless of host, including additional resolutions upto HD are available through GPU configuration and these can be selected and programmed on the host in languages such as Basic.</li>
  <li><font style="color: orange;" size="3">Sound</font><br>- the host will have access to the stereo DAC converters, which can playback 48KHz CD quality sound or emulate the SN76489 or basic bit/timer sound of the Sharp series. Sound recording is also possible via the Mic input.</li>
  <li><font style="color: orange;" size="3">Processors</font><br>- there are many software CPU implementations which can be ported to run on this platform, for example the ARM platform CPU emulations of the BBC PiCoPro can readily be ported. This in turn allows the potential for other machines, using the SOM advanced graphics and sound as necessary, allow emulations of machines such as the BBC to run on this Sharp host.</li>
  <li><font style="color: orange;" size="3">Linux</font><br>- using the host keyboard, speaker, monitor etc, a full blooded version of Linux, including Wifi, can be utilised at the host console.</li>
</font></ul>

</div>


--------------------------------------------------------------------------------------------------------

## <font style="color: yellow;" size="6">Hardware</font>


<div style="text-align: justify; line-height: 1.4em; font-size: 0.8em; font-family:sans-serif;">
Version 1.0 is the first official release of the tran<i>ZPU</i>ter<sup><i>FusionX</i></sup> design.
<div style="padding-top: 0.8em;"></div>

The <i>FusionX</i> board builds on a tried and tested Z80 host interface using the Altera 7000A MAX CPLD device. The CPLD not only interfaces the 5V Z80 host signals to the 3.3V signals on more recent devices, it also embeds the logic to perform accurate Z80 timing using a 50MHz clock to
sample the Z80 host clock and activate signals according to the published Z80 state diagrams.
<div style="padding-top: 0.8em;"></div>

In addition the <i>FusionX</i> includes a SigmaStar System-On-a-Module, this is a small 29mmx29mm stamp device which incorporates a dual-core Cortex-A7 CPU, 128MByte DRAM, 256MBytes FlashNAND and a Wifi transceiver. The SigmaStar SOM is capable of outputting 2D Graphics in an RGB 888 format
with selectable resolution upto HD format. It is also capable of stereo audio DAC output at 48KHz. Click to view the full SigmaStar <a href='../Downloads/SSD202D_ProductBrief_v01.pdf'>product brief</a>.
<div style="padding-top: 0.8em;"></div>

Using the experience gathered on the tranZPUter SW-700, a 30bit Video DAC is chosen to render the SigmaStar SOM video rather than a 2R-R ladder and additionally an 8bit DAC is included for rendering monochrome monitor contrast levels to cater for colour shading on monochrome CRT 
monitors as found in the MZ-80A/MZ-2000.
<div style="padding-top: 0.8em;"></div>

The hardware design centers on a main circuit board which holds all the primary circuitry and a number of daughter boards, each daughter board dedicated to one host (ie. MZ-700, MZ-80A, MZ-2000). The daughter board intercepts the host video/audio subsystem and supports switching
of the host video/audio and the mainboard video/audio to the monitor/speaker of the host. The mainboard can be used without daughter boards, the latter are only used when the SOM Video/Audio is required.
<div style="padding-top: 0.8em;"></div>

This section outlines the mainboard schematics and circuit board design of the tran<i>ZPU</i>ter<sup><i>FusionX</i></sup>.
</div>

## <font style="color: yellow;" size="5">Schematics</font>

<div style="padding-top: 0.8em;"></div>

<font style="color: cyan;" size="4">Schematic 1 - Z80 Host socket to CPLD</font>
<div style="text-align: justify; line-height: 1.4em; font-size: 0.8em; font-family:sans-serif;">
The schematic interfaces the Z80 to the CPLD. The CPLD is 5V tolerant and operates internally at 3.3V. The outputs are selected as Low Voltage TTL meaning a '1' is represented by 3.3V as opposed to 5V in the host system. The specifications of 5V TTL see the switching threshold at approx 2.4V,
thus the CPLD is able to drive 5V circuitry and with sufficient drive current, 25mA per pin.
<div style="padding-top: 0.8em;"></div>

The CPLD internal state machines are clocked by an external 50MHz oscillator, this allows for adequate sampling and state change for a typical 1MHz - 6MHz Z80 host.
</div>

![FusionX Schematic2](http://eaw.app/images/tzpuFusionX_v1_0_Schematics-2.png)

<font style="color: cyan;" size="4">Schematic 2 - I/O (Audio, UART, USB)</font>
<div style="text-align: justify; line-height: 1.4em; font-size: 0.8em; font-family:sans-serif;">
The SOM is rich in peripherals and this circuit interfaces some of them for use in the FusionX, these include:

<ul style="line-height: 0.9em;"><font size="3">
<li><font style="color: orange;" size="3">Stereo Audio Microphone input</font><br>- a digital microphone input is also available but the pins are used in the CPLD interface.</li>
<li><font style="color: orange;" size="3">Stereo Audio DAC output</font><br>- dual digital to analogue converters for sound output which can be clocked at 48KHz.</li>
<li><font style="color: orange;" size="3">WiFi Antenna</font><br>- a SSW101B 20/40MHz IEEE 802.11 b/g/n/e/l/n/w WiFi transceiver operating in the 2.4GHz band with a 500M range. The SOM also includes a 100MHz ETH PHY but this is not used in this design as hard wired ethernet is not practical for a board which is
sited inside a retro machine.</li>
<li><font style="color: orange;" size="3">USB Serial</font><br>- when running Linux, the console is presented on a UART serial device. This serial device is converted into USB for ease of use to view and connect with the Linux console.</li>
<li><font style="color: orange;" size="3">USB</font><br>- a Linux connected USB port allowing for device expansion, such as additional storage, mice etc.</li>
<li><font style="color: orange;" size="3">Fast UART</font><br>- high speed full duplex with hardware handshake UART.</li>
<li><font style="color: orange;" size="3">UART</font><br>- standard 2 pin UART operating upto 500KHz.</li>
<li><font style="color: orange;" size="3">SD Card</font><br>- the SOM has inbuilt FlashNAND so can accomodate a simple Linux filesystem, addition of an SD card allows for greater storage of Host applications and Linux utilities. An SD card also makes for ease of upgrades as the SOM will auto upgrade when a suitably prepared SD card is present on boot.</li>
</font></ul>

</div>

![FusionX Schematic2](http://eaw.app/images/tzpuFusionX_v1_0_Schematics-3.png)

<font style="color: cyan;" size="4">Schematic 3 - Video (VideoDAC, Contrast DAC)</font>

<div style="text-align: justify; line-height: 1.4em; font-size: 0.8em; font-family:sans-serif;">
The SOM outputs TTL RGB with 8bits per colour. This is interfaced to a 30bit VideoDAC with the lowest 2 bits, per colour, controlled by the CPLD. 
<div style="padding-top: 0.8em;"></div>

In addition, in order to drive the internal monochrome monitors of the Sharp MZ-80A/MZ-80B/MZ-2000 an 8bit VideoDAC is added which outputs a video signal in the range 4V-5V using a 332 RGB colour input, the colour input being the MSB of the SOM 888 TTL output. I term this the Contrast
DAC, as it is sending the video signal with colour information as a voltage controlled contrast signal which presents itself on the monitor as differing contrast levels, thus simulating colour as grey levels.
<div style="padding-top: 0.8em;"></div>

In order to get true black, the CPLD creates a blanking signal, MONO.BLANK, which is paired with a MUX 0V clamp on the daughter board which drives the monochrome monitor, this sees the RGB332 as 0V when 00000000 is present, then varying between 4.01V-5V when non-zero.
</div>

![FusionX Schematic4](http://eaw.app/images/tzpuFusionX_v1_0_Schematics-4.png)

<font style="color: cyan;" size="4">Schematic 4 - Power Supply (3.3V, USB)</font>

<div style="text-align: justify; line-height: 1.4em; font-size: 0.8em; font-family:sans-serif;">
The power supply, for the SOM and CPLD, converts the 5V present on the Z80 socket into 3.3V using a high efficiency buck converter. This is necessary to minimise heat and provide maximum current to the SOM/CPLD.
<div style="padding-top: 0.8em;"></div>

Additionally, a software controlled USB power switch is installed to enable (and reset if required) +5V power to the USB expansion port.
</div>

![FusionX Schematic5](http://eaw.app/images/tzpuFusionX_v1_0_Schematics-5.png)

<font style="color: cyan;" size="4">Schematic 5 - CPLD Interface</font>

<div style="text-align: justify; line-height: 1.4em; font-size: 0.8em; font-family:sans-serif;">
The final schematic is the interface between the SOM and the CPLD. Originally this was going to be a bi-directional 16bit bus with Read/Write and Strobe signals but after testing, the setup time for a 16bit signal with tri-state switching was much slower than an SPI connection
due to the GPIO register layout and operation within the SOM and the speed of the I/O operations within the SOM.
<div style="padding-top: 0.8em;"></div>
The solution used is to have a bidirectional 72MHz SPI bus between the SOM and CPLD for transmitting Z80 transaction requests and an 8bit read only parallel bus for more rapid reading of Z80 Data and seperate Z80 state information.
</div>

![FusionX Schematic6](http://eaw.app/images/tzpuFusionX_v1_0_Schematics-6.png)

#### <font style="color: yellow;" size="5">PCB

<div style="text-align: justify; line-height: 1.4em; font-size: 0.8em; font-family:sans-serif;">
The PCB was designed with minimum size as a primary requirement for the various machines in which it would be installed. It also had to be compatible with the tran<i>ZPU</i>ter<sup><i>Fusion</i></sup> for interchangeability.
<div style="padding-top: 0.8em;"></div>

A major concern was heat dissipation as the PCB, when installed within an MZ-700 is very close to existing motherboard components which give off a lot of heat with no air circulation in a sealed compact housing. This meant active components couldnt be sited on the PCB underside
as heat generation would lead to instability and failure, which in turn led to an increase in the final PCB size. 
<div style="padding-top: 0.8em;"></div>

The smallest components which could be manually assembled where used, ie. 0402/0603 passive devices and 0.5mm IC pitch spacing to reduce overall size and a 4 layer stackup selected to fit all required components.
<div style="padding-top: 0.8em;"></div>
</div>

<font style="color: cyan;" size="4">PCB Top Overview</font>

![FusionX PCB Top](http://eaw.app/images/tzpuFusionX_v1_0_3D_Top.png)

<font style="color: cyan;" size="4">PCB Bottom Overview</font>

![FusionX PCB Bottom](http://eaw.app/images/tzpuFusionX_v1_0_3D_Bottom.png)

<font style="color: cyan;" size="4">PCB 4 Layer Routing Overview</font>

![FusionX Routing](http://eaw.app/images/tzpuFusionX_v1_0_layout.png)

<font style="color: cyan;" size="4">PCB Assembled</font>

![FusionX Assembled](http://eaw.app/images/FusionX_Top.png)

![FusionX Assembled](http://eaw.app/images/FusionX_Bottom.png)


<font style="color: cyan;" size="4">PCB Component Placement and Bill of Materials</font>

<div style="text-align: justify; line-height: 1.4em; font-size: 0.8em; font-family:sans-serif;">
Click <a href="../_pages/FusionX_v1_0_BOM.html">here</a> to view an interactive PCB component placement diagram and Bill of Materials.
</div>

--------------------------------------------------------------------------------------------------------

## <font style="color: yellow;" size="6">Software</font>

Under construction.


###### <font style="color: yellow;" size="5">Linux</font>

Under construction.

###### <font style="color: yellow;" size="5">Z80 Emulator</font>

Under construction.

--------------------------------------------------------------------------------------------------------

## <font style="color: yellow;" size="6">Daughter Boards</font>

<div style="text-align: justify; line-height: 1.4em; font-size: 0.8em; font-family:sans-serif;">
The tranZPUter series was initially developed in the Sharp MZ-80A and was primarily a Z80 replacement. As the concept evolved and the tranZPUter SW-700 was developed for the MZ-700 it became more of an integral component of the machine, offering original and upgraded Video and Audio capabilites by intercepting and routing existing signals.
<div style="padding-top: 0.8em;"></div>

After significant developments on the tranZPUter SW-700 it became desirable to port it back to the MZ-80A and MZ-2000 but these machines had different CPU orientation and signal requirements, ie. driving an internal and external monitor. This requirement led to the concept of daughter boards, where a specific board would be designed and developed for
the target host and would plug into the tranZPUter SW-700 card. Ideally I wanted to port the SW-700 to an MZ-800/MZ-1500 and X1 but the size of the card and orientation of the Z80 was a limitation.
<div style="padding-top: 0.8em;"></div>

During the design of the tran<i>ZPU</i>ter<sup><i>FusionX</i></sup> one of the main requirements was to make the board small, the Z80 orientation changeable and also compatible with the tran<i>ZPU</i>ter<sup><i>Fusion</i></sup> so that it could fit many machines and be interchangeable. As the SW-700 also interfaced to the Video and Audio of the machines 
and each was quite different, it became apparent that the tran<i>ZPU</i>ter<sup>FusionX</sup> needed to include a concept to allow different video/audio interfaces according to the targetted host. This concept was realised via daughter boards. Two connectors would link the tran<i>ZPU</i>ter<sup><i>FusionX</i></sup> to a daughter board which would be 
specifically designed for the intended host.
<div style="padding-top: 0.8em;"></div>

The daughter boards would be responsible for switching and mixing video/audio signals and to drive internal monitors and provide the correct input and output connectors for ease of installation.
<div style="padding-top: 0.8em;"></div>

Currently three daughter boards have been developed, for the MZ-700, MZ-80A and MZ-2000 and more will follow as the design progresses.
</div>

--------------------------------------------------------------------------------------------------------

###### <font style="color: yellow;" size="5">MZ-700 Daughter Board</font>

<div style="text-align: justify; line-height: 1.4em; font-size: 0.8em; font-family:sans-serif;">
The purpose of the MZ-700 daughter board is to interface the video/audio circuits of the <i>FusionX</I> board with those of the MZ-700. It is designed to be inserted into the mainboard modulator output and the modulator connector in turn connected to the daughter board. This allows the MZ-700 to be used with a standard monitor and the video output 
is switched between the MZ-700 and <i>FusionX</i> under control of the <i>FusionX</i>.
<div style="padding-top: 0.8em;"></div>

The original sound circuity of the MZ-700 drives a speaker directly and in order to inject <i>FusionX</i> audio into the MZ-700 speaker, the mainboard speaker output is routed to the daughter board, level converted and switched under control of the <i>FusionX</I>. The <i>FusionX</i> offers stereo sound so this is selectively switched/mixed with the
original MZ-700 sound and fed to a Class D amplifier which then drives the internal speaker. Line level stereo output is achieved via an additional 4pin connector and used as required.
<div style="padding-top: 0.8em;"></div>

This setup allows for Linux or emulated machines, whilst running as an application on the <i>FusionX</i>, to output their sound to the internal speaker.
<div style="padding-top: 0.8em;"></div>
</div>


<font style="color: cyan;" size="4">MZ-700 Video Interface Schematic</font>

<div style="text-align: justify; line-height: 1.4em; font-size: 0.8em; font-family:sans-serif;">
The MZ-700 daughter board consists of three 4way SPDT analogue switches to route video and audio signals under <i>FusionX</i> control and a Class D power amplifier.
</div>

![MZ700 VideoInterface Schematic6](http://eaw.app/images/VideoInterface_MZ700_V1_0_Schematics-1.png)

<font style="color: cyan;" size="4">MZ-700 Video Interface PCB</font>

<div style="text-align: justify; line-height: 1.4em; font-size: 0.8em; font-family:sans-serif;">
The MZ-700 daughter board PCB is small and compact due to the space restrictions. It has to fit onto the existing modulator connector and within the available free space.
</div>

<div style='content: ""; clear: both; display: table;'>
    <div style='width: 35%; padding: 5px; padding-left: 1em; float: left'>
        <img src='http://eaw.app/images/VideoInterface_MZ700_v1_0_3D_Top.png' height='100%' width='100%' style="margin-left: auto; margin-right: auto; display: block;"/>
    </div>
    <div style='width: 39%; padding: 5px; padding-left: 1em; float: left'>
        <img src='http://eaw.app/images/VideoInterface_MZ700_v1_0_3D_Bottom.png' height='100%' width='100%' style="margin-left: auto; margin-right: auto; display: block;"/>
    </div>
</div>

<div style='content: ""; clear: both; display: table;'>
    <div style='width: 80%; padding: 5px; padding-left: 1em; float: left'>
        <img src='http://eaw.app/images/FusionX_MZ700.png' height='100%' width='100%' style="margin-left: auto; margin-right: auto; display: block;"/>
    </div>
</div>

--------------------------------------------------------------------------------------------------------

###### <font style="color: yellow;" size="5">MZ-2000 Daughter Board</font>

<div style="text-align: justify; line-height: 1.4em; font-size: 0.8em; font-family:sans-serif;">
The purpose of the MZ-2000 daughter board is to interface the video/audio/reset circuits of the <i>FusionX</I> board with those of the MZ-2000. The MZ-2000 has an internal monochrome CRT, external RGB video and internal Audio with an amplifier on the monochrome CRT control board.
<div style="padding-top: 0.8em;"></div>

The daughter board is designed to be inserted simultaneously into the mainboard monitor and IPL connectors. It presents all the required connectors to connect the IPL/RESET switches, internal monitor and external monitor on the same board.
<div style="padding-top: 0.8em;"></div>

The IPL and RESET inputs are intercepted on the daughter board and sent to the <i>FusionX</i> as the MZ-2000 operates in different modes dependent on which RESET key is pressed during a Z80 Reset.
<div style="padding-top: 0.8em;"></div>

The video signals from the mainboard are switched with the <i>FusionX</i> video monochrome signals and sent to the internal CRT monitor. This allows for original video output on the CRT monitor or advanced <i>FusionX</i> text and graphics, resolution subject to the timing constraints of the monitor.
<div style="padding-top: 0.8em;"></div>

The <i>FusionX</i> RGB output is routed to the MZ-2000 external RGB video socket allowing for upto full HD external colour video display.
<div style="padding-top: 0.8em;"></div>

The sound circuity of the MZ-2000 is sent to an audio amplifier on the CRT monitor. This signal is intercepted and switched with the <i>FusionX</i> audio which then drives the CRT monitor amplifier. Line level stereo output is achieved via an additional 4pin connector and used as required.
<div style="padding-top: 0.8em;"></div>

</div>

<font style="color: cyan;" size="4">MZ-2000 Video Interface Schematic</font>

<div style="text-align: justify; line-height: 1.4em; font-size: 0.8em; font-family:sans-serif;">
The MZ-2000 daughter board consists of two 4way SPDT analogue switches to route video and audio signals under <i>FusionX</i> control. One SPDT switch is used for creating pure black in the modified video signal generated on the <i>FusionX</i> board.
</div>

![MZ2000 VideoInterface Schematic6](http://eaw.app/images/VideoInterface_MZ2000_V1_0_Schematics-1.png)

<font style="color: cyan;" size="4">MZ-2000 Video Interface PCB</font>

<div style="text-align: justify; line-height: 1.4em; font-size: 0.8em; font-family:sans-serif;">
The MZ-2000 daughter board PCB is small and compact due to the space restrictions and the number of connectors it must carry. It plugs into the mainboard monitor/IPL male connectors and then presents new CRT monitor, external Video and IPL/RESET switch connectors for all the exising internal cabling.
</div>

<div style='content: ""; clear: both; display: table;'>
    <div style='width: 50%; padding: 5px; padding-left: 1em; float: left'>
        <img src='http://eaw.app/images/VideoInterface_MZ2000_v1_0_3D_Top.png' height='100%' width='100%' style="margin-left: auto; margin-right: auto; display: block;"/>
    </div>
    <div style='width: 50%; padding: 5px; padding-left: 1em; float: left'>
        <img src='http://eaw.app/images/VideoInterface_MZ2000_v1_0_3D_Bottom.png' height='100%' width='100%' style="margin-left: auto; margin-right: auto; display: block;"/>
    </div>
</div>

<div style='content: ""; clear: both; display: table;'>
    <div style='width: 60%; padding: 5px; padding-left: 1em; float: left'>
        <img src='http://eaw.app/images/FusionX_MZ2000.png' height='80%' width='100%' style="margin-left: auto; margin-right: auto; display: block;"/>
    </div>
</div>

--------------------------------------------------------------------------------------------------------

###### <font style="color: yellow;" size="5">MZ-80A Daughter Board</font>

<div style="text-align: justify; line-height: 1.4em; font-size: 0.8em; font-family:sans-serif;">
The purpose of the MZ-80A daughter board is to interface the video/audio/reset circuits of the <i>FusionX</I> board with those of the MZ-80A. The MZ-80A has an internal monochrome CRT, cutouts for and external RGB video socket and internal Audio with an amplifier on the monochrome CRT control board.
<div style="padding-top: 0.8em;"></div>

The daughter board is designed to plug into the vertical mainboard CRT video connector with a gap so that the data cassette connector can be simultaneously connected. The gap is necessary as the CRT video connector sits close to the rear sidewall so the daughter board must extend forwards towards the keyboard. 
<div style="padding-top: 0.8em;"></div>

It presents all the required connectors to connect the RESET switch (both in and out), internal monitor and external monitor on the same board.
<div style="padding-top: 0.8em;"></div>

The RESET input is intercepted on the daughter board and sent to the <i>FusionX</i>. Technically it isnt needed as the <i>FusionX</i> samples the Z80 Reset which is based on this input, but it can be useful, for example, detecting requests to reboot the SOM (double press) rather than the MZ-80A circuitry.
<div style="padding-top: 0.8em;"></div>

The video signals from the mainboard are switched with the <i>FusionX</i> video monochrome signals and sent to the internal CRT monitor. This allows for original video output on the CRT monitor or advanced <i>FusionX</i> text and graphics, resolution subject to the timing constraints of the monitor.
<div style="padding-top: 0.8em;"></div>

The <i>FusionX</i> RGB output is routed to the MZ-80A external RGB video socket (if installed) allowing for upto full HD external colour video display.
<div style="padding-top: 0.8em;"></div>

The sound circuity of the MZ-80A is sent to an audio amplifier on the CRT monitor. This signal is intercepted and switched with the <i>FusionX</i> audio which then drives the CRT monitor amplifier. Line level stereo output is achieved via an additional 4pin connector and used as required.
<div style="padding-top: 0.8em;"></div>

</div>

<font style="color: cyan;" size="4">MZ-80A Video Interface Schematic</font>

<div style="text-align: justify; line-height: 1.4em; font-size: 0.8em; font-family:sans-serif;">
The MZ-80A daughter board consists of two 4way SPDT analogue switches to route video and audio signals under <i>FusionX</i> control. One SPDT switch is used for creating pure black in the modified video signal generated on the <i>FusionX</i> board.
</div>

![MZ80A VideoInterface Schematic6](http://eaw.app/images/VideoInterface_MZ80A_V1_0_Schematics-1.png)

<font style="color: cyan;" size="4">MZ-80A Video Interface PCB</font>

<div style="text-align: justify; line-height: 1.4em; font-size: 0.8em; font-family:sans-serif;">
The MZ-80A daughter board PCB is small and compact with a large punchout to enable connection with the mainboard CRT connector and thru connection of the data cassette signal connector. It plugs into the mainboard CRT monitor connector and then presents new CRT monitor, external Video and RESET In/Out switch connectors to be used with all the exising internal cabling.
</div>

<div style='content: ""; clear: both; display: table;'>
    <div style='width: 47%; padding: 5px; padding-left: 1em; float: left'>
        <img src='http://eaw.app/images/VideoInterface_MZ80A_V1_0_3D_Top.png' height='100%' width='100%' style="margin-left: auto; margin-right: auto; display: block;"/>
    </div>
    <div style='width: 50%; padding: 5px; padding-left: 1em; float: left'>
        <img src='http://eaw.app/images/VideoInterface_MZ80A_V1_0_3D_Bottom.png' height='100%' width='100%' style="margin-left: auto; margin-right: auto; display: block;"/>
    </div>
</div>
<div style="text-align: justify; line-height: 1.4em; font-size: 0.8em; font-family:sans-serif;">


</div>

--------------------------------------------------------------------------------------------------------

## <font style="color: yellow;" size="5">Reference Sites</font>

<div style="text-align: justify; line-height: 1.4em; font-size: 0.8em; font-family:sans-serif;">
The table below contains all the sites referenced in the design and programming of the tran<i>ZPU</i>ter<sup><i>FusionX</i></sup>
</div>

<div style="padding-top: 0.8em;"></div>
<div style="padding-left: 10%;">
<font size="4">
<table>
  <thead>
    <tr>
      <th>Site</th>
      <th>Language</th>
      <th>Description</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td><a href="https://github.com/redcode/Z80">Z80 Emulation</a></td>
      <td>English</td>
      <td>A highly accurate Z80 Emulation written in C, the heart of the FusionX.</td>
    </tr>
    <tr>
      <td><a href="https://whycan.com/t_6006.html">WhyCan Forum</a></td>
      <td>Chinese</td>
      <td>Invaluable Forum with threads on SigmaStar products.</td>
    </tr>
    <tr>
      <td><a href="http://doc.industio.com/docs/ssd20x-system/page_0">SSD20X System Development Manual</a></td>
      <td>Chinese</td>
      <td>System development manual for the SSD20X CPU.</td>
    </tr>
    <tr>
      <td><a href="https://wx.comake.online/doc/d8clf27cnes2-SSD20X/customer/development/arch/arch.html">SigmaStarDocs</a></td>
      <td>Chinese</td>
      <td>SDK and API development manual.</td>
    </tr>
    <tr>
      <td><a href="http://doc.industio.com/docs/ido-som2d0x-start/ido-som2d0x-start-1ctpf8rg06ct0">SOM2D0X Beginners Guide</a></td>
      <td>Chinese</td>
      <td>Beginners Guide to the SOM2D0X.</td>
    </tr>
    <tr>
      <td><a href="https://github.com/civetweb/civetweb/blob/master/docs/UserManual.md">CivetWeb Users Manual</a></td>
      <td>English</td>
      <td>User Manual for the CivetWeb Embedded Web Server.</td>
    </tr>
  </tbody>
</table>
</font></div>

--------------------------------------------------------------------------------------------------------

## <font style="color: yellow;" size="5">Manuals and Datasheets</font>

<div style="text-align: justify; line-height: 1.4em; font-size: 0.8em; font-family:sans-serif;">
The table below contains all the datasheets and manuals referenced in the design and programming of the tran<i>ZPU</i>ter<sup><i>FusionX</i></sup>
</div>

<div style="padding-top: 0.8em;"></div>
<div style="padding-left: 10%;">
<font size="4">
<table>
  <thead>
    <tr>
      <th>Datasheet</th>
      <th>Language</th>
      <th>Description</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td><a href="/Downloads/ADV7123_Datasheet.pdf">ADV7123</a></td>
      <td>English</td>
      <td>Original 5V 30bit VideoDAC (discontinued)</td>
    </tr>
    <tr>
      <td><a href="/Downloads/GOTECOM-GM7123_VideoDAC.pdf">GM7123</a></td>
      <td>Chinese</td>
      <td>Chinese 3.3V version of the ADV7123 30bit VideoDAC converter.</td>
    </tr>
    <tr>
      <td><a href="/Downloads/CH340E_Datasheet.pdf">CH340E</a></td>
      <td>Chinese</td>
      <td>USB to Serial UART converter.</td>
    </tr>
    <tr>
      <td><a href="/Downloads/EPM7512AEQFP144_Datasheet.pdf">EPM7512AEQFP144</a></td>
      <td>English</td>
      <td>Altera 512 MacroCell 5V tolerant CPLD.</td>
    </tr>
    <tr>
      <td><a href="/Downloads/HXJ8002_Datasheet.pdf">HXJ8002</a></td>
      <td>English</td>
      <td>Class D power amplifier.</td>
    </tr>
    <tr>
      <td><a href="/Downloads/IDO_SOM2D01_Datasheet_EN.pdf">SOM2D01</a></td>
      <td>English</td>
      <td>SigmaStar SOM Datasheet (original model).</td>
    </tr>
    <tr>
      <td><a href="/Downloads/REF3040_Datasheet.pdf">REF3040</a></td>
      <td>English</td>
      <td>Precise 4V reference voltage generator.</td>
    </tr>
    <tr>
      <td><a href="/Downloads/SY6280_Datasheet.pdf">SY6280</a></td>
      <td>English</td>
      <td>Power distribution switch, used for enabling and supplying USB Bus power.</td>
    </tr>
    <tr>
      <td><a href="/Downloads/TLC5602C_Datasheet.pdf">TLC5602C</a></td>
      <td>English</td>
      <td>8bit VideoDAC converter.</td>
    </tr>
    <tr>
      <td><a href="/Downloads/TLV62569_Datasheet.pdf">TLV62569</a></td>
      <td>English</td>
      <td>High efficiency Buck Converter.</td>
    </tr>
    <tr>
      <td><a href="/Downloads/TMUX1134_Datasheet.pdf">TMUX1134</a></td>
      <td>English</td>
      <td>Precision SPDT Analogue switch (Mux).</td>
    </tr>
    <tr>
      <td><a href="/Downloads/ESD_Diode_vcut0714bhd1.pdf">VCUT0714BHD1</a></td>
      <td>English</td>
      <td>ESD Protection Diode.</td>
    </tr>
    <tr>
      <td><a href="/Downloads/SigmaStar_USB_Programmer.docx">USB Programmer</a></td>
      <td>English</td>
      <td>SigmaStar USB Programmer for SSD202 Processor.</td>
    </tr>
    <tr>
      <td><a href="/Downloads/SSD201_HW_Checklist_V10.xlsx">SSD201 HW Checklist v10</a></td>
      <td>English</td>
      <td>SigmaStar SSD201 Hardware Checklist.</td>
    </tr>
    <tr>
      <td><a href="/Downloads/SSD202D_Reference_v04.pdf">SSD202D Reference v04</a></td>
      <td>English</td>
      <td>SigmaStar SSD202 CPU Reference Manual.</td>
    </tr>
    <tr>
      <td><a href="/Downloads/IDO_SOM2D02_Pinout.xlsx">SOM2D02_Pinout</a></td>
      <td>English</td>
      <td>SigmaStar SOM2D02 Pinout.</td>
    </tr>
    <tr>
      <td><a href="/Downloads/Z80_UserManual.pdf">Z80 UserManual</a></td>
      <td>English</td>
      <td>Z80 User Manual.</td>
    </tr>
    <tr>
      <td><a href="/Downloads/SSD202D_ProductBrief_v01.pdf">SSD202D Product Brief</a></td>
      <td>English</td>
      <td>SigmaStar SSD202 CPU Product Brief.</td>
    </tr>
    <tr>
      <td><a href="/Downloads/IDO_SOM2D01_Datasheet_EN.pdf">SOM2D01 Datasheet</a></td>
      <td>English</td>
      <td>SigmaStar SOM2D01 Datasheet.</td>
    </tr>
  </tbody>
</table>
</font></div>

--------------------------------------------------------------------------------------------------------

## <font style="color: yellow;" size="5">Demonstration Videos</font>

<font style="color: cyan;" size="4">MZ-2000 Demo</font>

<a href="https://t.co/ZP4T3oisrg">MZ-2000 Demo</a>

<font style="color: cyan;" size="4">MZ-700 Demo</font>

<a href="https://t.co/6lJoGkNuiP">MZ-700 Demo</a>


--------------------------------------------------------------------------------------------------------

## <font style="color: yellow;" size="5">Credits</font>

<div style="text-align: justify; line-height: 1.4em; font-size: 0.8em; font-family:sans-serif;">
The Z80 Emulation used in the <i>FusionX</i> is (c) 1999-2022 Manuel Sainz de Baranda y Goñi, which is licensed under the LGPL v3 license, source can be found in <a href="https://github.com/redcode/Z80">github</a>.
<div style="padding-top: 0.8em;"></div>

The SSD202/SOM2D0X build system is based on Linux with extensions by SigmaStar and Industio, licensing can be found in their updated source files.
</div>

--------------------------------------------------------------------------------------------------------

## <font style="color: yellow;" size="5">Licenses</font>

<div style="text-align: justify; line-height: 1.4em; font-size: 0.8em; font-family:sans-serif;">
This design, hardware and software (attributable components excluding seperately licensed software) is licensed under the GNU Public Licence v3 and free to use, adapt and modify by individuals, groups and educational institutes.
<div style="padding-top: 0.8em;"></div>

No commercial use to be made of this design or any hardware/firmware component without express permission from the author.
</div>

--------------------------------------------------------------------------------------------------------

## <font style="color: yellow;" size="5">The Gnu Public License v3</font>

<div style="text-align: justify; line-height: 1.4em; font-size: 0.8em; font-family:sans-serif;">
 The source and binary files in this project marked as GPL v3 are free software: you can redistribute it and-or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
<br><br>

 The source files are distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
<br><br>

 You should have received a copy of the GNU General Public License along with this program.  If not, see http://www.gnu.org/licenses/.
</div>

--------------------------------------------------------------------------------------------------------

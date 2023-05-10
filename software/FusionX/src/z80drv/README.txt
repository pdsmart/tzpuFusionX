Development Cycle
-----------------

z80drv now supports several machines, ie. MZ-80A, MZ-700, MZ-2000, PCW-8256 with more in the pipeline. 

Please edit Makefile and set the model prior to changing the Model source. Alternatively, isse make with the host name, ie. make MZ80A.

The virtual device concept (ie. an MZ-700 with a tranZPUter SW card) has been incorporated as virtual hardware modules, ie. z80vhw_rfs.c
which adds the RFS card to the MZ-700/MZ-80A. These virtual devices are built and added to the target kernel module dependent on supported
host.

Zeta
----
The Zeta library includes spaces in the naming of certain header files and directories. The linux kernel module build system kbuild
cant work with spaces, at least I have found no work around, so after making a submodule pull on Zeta you need to manually rename or edit the files, ie:

	Manually rename the files:
	mv Zeta/API/Z/inspection/data\ model Zeta/API/Z/inspection/data_model
	mv Zeta/API/Z/formats/data\ model Zeta/API/Z/formats/data_model
	mv Zeta/API/Z/keys/data\ model.h Zeta/API/Z/keys/data_model.h

	Edit the files and change 'data model' to data_mode
	vi Zeta/API/Z/types/integral.h
	vi Zeta/API/Z/inspection/data_model.h

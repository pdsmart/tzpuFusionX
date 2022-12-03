Development Cycle
-----------------

To avoid changes for one host affecting another the driver is split into seperate hosts containing almost identical code.
The idea, after development is complete, is to merge them into a single drive which autodetects, via the CPLD, the host model
and selects the code accordingly.

Please edit Makefile and set the model prior to changing the Model source. In theory this file will be deleted once the source is
merged.

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

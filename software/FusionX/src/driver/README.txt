Development Cycle
-----------------

To avoid changes for one host affecting another the driver is split into seperate hosts containing almost identical code.
The idea, after development is complete, is to merge them into a single drive which autodetects, via the CPLD, the host model
and selects the code accordingly.

Please edit Makefile and set the model prior to changing the Model source. In theory this file will be deleted once the source is
merged.

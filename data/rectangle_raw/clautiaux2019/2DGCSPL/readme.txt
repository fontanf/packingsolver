Instances are grouped by class to run them consecutively (e.g. BPP_C1)
Raw files are stored in bpp folder for the bin-packing problem and in cbpp folder for consecutive bin-packing problem.
A instance file in bpp folder is decomposed as follows:
	- first raw: number of items to cut, number of bins
	- one raw for each item: item height, item width, item demand
	- one raw for each bin: bin height, bin width

A instance file in cbpp folder is decomposed as follows:
	- first raw: number of batch
	- for each batch, information are stored as in a bin-packing problem:
		- first raw: number of items to cut, number of bins
		- one raw for each item: item height, item width, item demand
		- one raw for each bin: bin height, bin width

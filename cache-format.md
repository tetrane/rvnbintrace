Described in this file is the version 1.0 of the binary trace cache format.

# Format overview

------
Header
 - Page size
------
Cache points: Memory regions + CPU context
------
Index: Trace context ID -> cache point
------

# Section description

Sections are declares as a stream of bytes, with sizes. Each section starts with a `Section size` value, which store the size of the section _excluding this size parameter_.

## Header

8B: Section size
4B: Page size

## Cache points

This section contains cache points contiguously. Note that the register list must be comprehensive: no register can be left out. It's up to the cache writer and reader to ensure / check the consistency of this list with the corresponding trace.

8B: Section size
For each cache point (see count in Index section)
	2B: Register list size
	For each register:
	    2B: Register ID
	    2B: Register size
	    XB: Register content
	For each memory page defined in index:
	    Page size B: Memory region content

## Index

8B: Section size
8B: Cache point count
For each cache point:
	8B: Trace context ID (These IDs start at 1, 0 being the initial context)
	8B: Stream offset in trace file's events section (excluding section size)
	8B: CPU Stream offset in cache points section
	4B: Page count
	For each page:
		8B: Start address
		8B: Stream offset in cache points section

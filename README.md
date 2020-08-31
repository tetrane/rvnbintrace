# rvnbintrace: a library for execution traces

## License

This work is dual-licensed X11 and GPL 2.0.
You can choose between one of them if you use this work.

`SPDX-License-Identifier: X11 OR GPL-2.0-only`

## What's this

This library is meant to produce and then read system-wide execution traces. Those traces can be huge, and as such are
meant to be used alongside a cache file which, although not mandatory, do make random accesses in reasonable time a
possibility.

See `trace-format.md` and `cache-format.md` for a detailed description of the binary format.

The traces are stored as initial memory regions along with initial cpu register values, to which we'll apply numerical differences. In order to make the trace as small as possible, it is possible to store register differences either as
brand new values or as operations depending on the previous values (called register operation).

The traces are meant to be as self-contained as possible, and try no to require external knowledge to be read. As such,
they contain a description of the machine they correspond to which contains information like register names and sizes.

Each trace must also declare one of a fixed set of well-defined architectures. It is up to the library user to ensure
the machine description fits in the architecture.

## How to use

If you need to write a trace, use TraceWriter and CacheWriter objects.

If you need to read a trace, you must inherit from TraceReader and implement the few required callbacks. You can use CacheReader as is.

See these object's documentations for more information.

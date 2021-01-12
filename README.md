# Objsize

## What is this?

This is a small library for computing size of OCaml heap values.
It computes count of words used for values, count of values' headers,
maximal depth of values.  There are functions to get size of values
in bytes too. It is also possible to calculate the total amount of 
heap memory used by live values reachable from GC roots.


## How to use it?

See `src/objsize.mli` for documentation.


## How to compile/install it?


Using dune:

    dune build @install


## How does it work?

C-function walks through values and uses header's field "color"
to mark visited values, then restores original values' "color".
Colors are stored using rle-like compression to decrease memory
usage.

## Bugs?

  1. Some constant values (like lists of integers) are
     constructed at compile time and placed outside of both heaps,
     and size of these values will be returned as 0.

  2. Internal function is not fully tail-recursive,
     so generally it uses stack proportionally to the depth
     of the value.

     There is an optimization to handle long lists and some
     other datastructures: when objsize walks through the
     structured block, the goto is used instead of recursive
     call to walk into the last value that should be visited.

     This optimization is not general, and the best solution
     would be to use heap memory instead of stack memory to store
     "walk path", but I don't need it now (please contribute
     if you want).

  3. OCaml 3.11 has new implementation of heap.  Versions of
     objsize >= 0.12 work only with OCaml 3.11 heap, versions
     of objsize <= 0.11 work only with OCaml <= 3.10.2 heap.
     Runtime failure will be raised if you link objsize >= 0.12
     with OCaml < 3.11.

## License?

Dual: BSD (3-clause) / GPL.

## Changes?

-  0.1  - 2007-12-13 - Initial public release.
-  0.11 - 2007-12-14 - "configure" made right. Now it works on 64-bits too.
-  0.12 - 2009-04-08 - Works with OCaml 3.11, installs with findlib.
-  0.13 - 2009-09-01 - Tiny change about so/dll suffix for unix/windows.
-  0.14 - 2010-01-26 - Fixing so/dll again.
                       Some stack usage optimization,
                       see the modified Bug #2 description.
-   0.15 - 2010-04-15 - Fix bug introduced in 0.14. (thanks to Steven Ramsay)
-   0.16 - 2010-08-11 - Fix bug introduced in 0.14. (thanks to SerP)
-   0.17 - 2014-04-23 - New functions objsize_roots and objsize_limit. (ygrek)
-   0.18 - 2016-05-01 - OCaml 4.03 compatibility

## Author?

Dmitry Grebeniuk <gdsfh1 at gmail dot com>

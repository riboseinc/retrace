# escape-hunting: VFS-escape detection end to end

The cookbook recipe 33 flow as a runnable demo (TODO.windows/04
examples parity). A packaged app (escape-demo) reads files from
a virtualized prefix; the VFS (simulated by `inside.json`)
materialized only the declared tree. One read is undeclared --
the escape.

## Layers
| Layer  | This demo                          |
|--------|------------------------------------|
| VFS    | inside.json (tfs-shaped)           |
| libc   | retrace preload (POSIX run script) |
| kernel | procmon CSV (Windows flow)         |

## POSIX
```sh
./run-posix.sh <build-dir>
```
Builds nothing; needs a configured build tree. Prints the
outside trace, then the correlate report: one escape
(leaked.dat), exit code 1.

## Windows
See run-windows.md (procmon capture -> retrace-procmon2retrace
--pid -> retrace-correlate; both path spellings normalize).

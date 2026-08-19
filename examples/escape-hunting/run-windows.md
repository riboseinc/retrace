# Windows flow (procmon)

The same demo on Windows: the kernel layer (procmon) is the
honest capture (the ntdll hook set is TODO.windows/06).

1. Stage the prefix and run the demo while Procmon captures:
   ```sh
   mkdir C:\escape-demo\vfs-root
   echo declared-main > C:\escape-demo\vfs-root\entry.dat
   echo declared-vars > C:\escape-demo\vfs-root\settings.dat
   echo host-secret  > C:\escape-demo\vfs-root\leaked.dat
   escape-demo.exe C:\escape-demo\vfs-root
   ```
2. In Procmon: filter by Process Name = escape-demo.exe, then
   File > Save as CSV (ProcmonCSV.csv).
3. Convert, scope to the demo's pid, correlate:
   ```sh
   retrace-procmon2retrace --pid <PID> ProcmonCSV.csv outside.json
   retrace-correlate --inside inside.json --outside outside.json \
                     --prefix "C:/escape-demo/vfs-root"
   ```
   (`inside.json` with REPLACE_PREFIX substituted to the DOS
   form `C:\\escape-demo\\vfs-root` -- the correlator
   normalizes both spellings before the join.)

Expected: entry.dat/settings.dat covered; leaked.dat reported as
`escape .../leaked.dat func=CreateFile class=read`.

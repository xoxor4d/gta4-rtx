# GTAIV PDB generator

Builds `GTA4_Win32_Final.pdb` for `GTAIV.exe` (Complete Edition 1.2.0.59) from an IDA database, so Tracy, Visual Studio and other debuggers show game function names.

1. Open the GTAIV database in IDA and run `export_ida_functions.py` (File > Script file...).  
   It writes `gtaiv_functions.json` next to the database.
2. Build the PDB into the game folder:
   ```
   python make_pdb.py "<game>/GTAIV.exe" "<idb folder>/gtaiv_functions.json" "<game>"
   ```
3. Optional check that dbghelp loads it and names functions correctly (run from the game folder):
   ```
   python verify_pdb.py "<game>/GTAIV.exe" "<idb folder>/gtaiv_functions.json" -
   ```

Rerun steps 1 and 2 after renaming functions in IDA. The PDB only holds public symbols (function names and addresses), no types or source lines.

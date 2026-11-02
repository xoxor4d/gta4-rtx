# GTAIV PDB generator

Builds `GTA4_Win32_Final.pdb` for `GTAIV.exe` (Complete Edition 1.2.0.59) from an IDA database, so Tracy, Visual Studio and other debuggers show game function names.  
The scripts are in `tools/gtaiv_pdb`.

1. Open the GTAIV database in IDA and run `tools/gtaiv_pdb/export_ida_functions.py` (File > Script file...).  
   It writes `gtaiv_functions.json` next to the database.

2. Generate the PDB next to `GTAIV.exe`:
   `python "<repo>/tools/gtaiv_pdb/make_pdb.py" "<game>/GTAIV.exe" "<idb folder>/gtaiv_functions.json" "<game>"`

3. Optional: check that dbghelp loads the PDB and resolves function names. Run it from the game folder;
   the trailing `-` uses the default symbol search (working directory and `_NT_SYMBOL_PATH`) like Tracy does:
   `python "<repo>/tools/gtaiv_pdb/verify_pdb.py" "<game>/GTAIV.exe" "<idb folder>/gtaiv_functions.json" -`

Rerun steps 1 and 2 after renaming functions in IDA. The PDB only holds public symbols (function names and addresses), no types or source lines.

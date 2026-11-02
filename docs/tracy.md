# Profiling with Tracy

## Build
Build the `ReleaseTracy` configuration. It is `Release` with the [Tracy](https://github.com/wolfpld/tracy) 0.13.4 client compiled in; all other configurations contain no profiling code.  
Use the matching Tracy 0.13.x profiler.

For game function names, put `GTA4_Win32_Final.pdb` next to `GTAIV.exe` (see [gtaiv_pdb.md](gtaiv_pdb.md)).

## Launch
CPU sampling and context switches need the game to run as administrator. The affinity batch files already elevate themselves,
so add these lines right before `start GTAIV.exe` in the one you use:

```bat
set TRACY_SYMBOL_OFFLINE_RESOLVE=1
set TRACY_SAMPLING_HZ=2000
```

- `TRACY_SYMBOL_OFFLINE_RESOLVE=1` - the game only records module + offset of sampled frames, names are resolved afterwards (see below). Without it Tracy's symbol worker cannot keep up and most frames stay unresolved.
- `TRACY_SAMPLING_HZ` - samples per second (default 8000), 2000 keeps the overhead low.

## Capture
Start the game, connect the Tracy profiler to `localhost`, play, then save the trace.  
Stopping a capture from the profiler always shows a "Connection lost" popup while sampling; the data is still complete apart from the last pending lookups.

## Resolve names
```bat
set "_NT_SYMBOL_PATH=D:\Games\Grand Theft Auto IV Complete Edition\GTAIV"
"<tracy folder>\tracy-update.exe" -r "<trace>.tracy" "<trace>_resolved.tracy"
```

Set `_NT_SYMBOL_PATH` to your game folder (where the pdb is). Add `-v` for verbose output (slower). Open the `_resolved` trace.

## Reading samples
- Flame graph: switch to `Sampling`, tick only `Render` and `Main thread` under visible threads, `Limit range` to the frames of interest, keep `External: Frames` on (game functions have no source lines).
- Thread lines: green = running, red / blue = waiting, olive = woken up but waiting for a free core.
- Names of Windows DLL frames (`ReadConsoleA`, `RtlWalkFrameChain`, ...) are nearest export guesses, only game, mod and bridge names are reliable.

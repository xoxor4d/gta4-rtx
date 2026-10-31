"""loads an image with dbghelp (as Tracy does) and checks SymFromAddr against the exported function names

usage: verify_pdb.py <image.exe> <functions.json> <symbol search dir>
"""
import ctypes
import ctypes.wintypes as wt
import json
import random
import sys

image, functions_path, search_dir = sys.argv[1:4]
dbghelp = ctypes.WinDLL('dbghelp.dll')

class SYMBOL_INFO(ctypes.Structure):
    _fields_ = [('SizeOfStruct', wt.ULONG), ('TypeIndex', wt.ULONG), ('Reserved', ctypes.c_uint64 * 2), ('Index', wt.ULONG),
                ('Size', wt.ULONG), ('ModBase', ctypes.c_uint64), ('Flags', wt.ULONG), ('Value', ctypes.c_uint64),
                ('Address', ctypes.c_uint64), ('Register', wt.ULONG), ('Scope', wt.ULONG), ('Tag', wt.ULONG),
                ('NameLen', wt.ULONG), ('MaxNameLen', wt.ULONG), ('Name', ctypes.c_char * 1024)]

class IMAGEHLP_MODULE64(ctypes.Structure):
    _fields_ = [('SizeOfStruct', wt.DWORD), ('BaseOfImage', ctypes.c_uint64), ('ImageSize', wt.DWORD), ('TimeDateStamp', wt.DWORD),
                ('CheckSum', wt.DWORD), ('NumSyms', wt.DWORD), ('SymType', wt.DWORD), ('ModuleName', ctypes.c_char * 32),
                ('ImageName', ctypes.c_char * 256), ('LoadedImageName', ctypes.c_char * 256), ('LoadedPdbName', ctypes.c_char * 256),
                ('CVSig', wt.DWORD), ('CVData', ctypes.c_char * 780), ('PdbSig', wt.DWORD), ('PdbSig70', ctypes.c_byte * 16),
                ('PdbAge', wt.DWORD), ('PdbUnmatched', wt.BOOL), ('DbgUnmatched', wt.BOOL), ('LineNumbers', wt.BOOL),
                ('GlobalSymbols', wt.BOOL), ('TypeInfo', wt.BOOL), ('SourceIndexed', wt.BOOL), ('Publics', wt.BOOL),
                ('MachineType', wt.DWORD), ('Reserved', wt.DWORD)]

dbghelp.SymGetOptions.restype = wt.DWORD
dbghelp.SymSetOptions.argtypes = [wt.DWORD]
dbghelp.SymInitialize.argtypes = [wt.HANDLE, ctypes.c_char_p, wt.BOOL]
dbghelp.SymLoadModuleEx.argtypes = [wt.HANDLE, wt.HANDLE, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint64, wt.DWORD, ctypes.c_void_p, wt.DWORD]
dbghelp.SymLoadModuleEx.restype = ctypes.c_uint64
dbghelp.SymFromAddr.argtypes = [wt.HANDLE, ctypes.c_uint64, ctypes.POINTER(ctypes.c_uint64), ctypes.POINTER(SYMBOL_INFO)]
dbghelp.SymGetModuleInfo64.argtypes = [wt.HANDLE, ctypes.c_uint64, ctypes.POINTER(IMAGEHLP_MODULE64)]

process = wt.HANDLE(0x7A11)
dbghelp.SymSetOptions(dbghelp.SymGetOptions() | 0x10)  # SYMOPT_LOAD_LINES, like Tracy
if not dbghelp.SymInitialize(process, None if search_dir == '-' else search_dir.encode(), False):
    sys.exit(f'SymInitialize failed: {ctypes.GetLastError()}')

export = json.load(open(functions_path, encoding='utf-8'))
base = dbghelp.SymLoadModuleEx(process, None, image.encode(), None, export['imagebase'], 0, None, 0)
info = IMAGEHLP_MODULE64(SizeOfStruct=ctypes.sizeof(IMAGEHLP_MODULE64))
dbghelp.SymGetModuleInfo64(process, base, ctypes.byref(info))
sym_types = {0: 'none', 1: 'coff', 2: 'cv', 3: 'pdb', 4: 'export', 5: 'deferred', 6: 'sym', 7: 'dia', 8: 'virtual'}
print('module base', hex(base), 'symbols:', sym_types.get(info.SymType, info.SymType), 'pdb:', info.LoadedPdbName.decode(),
      'unmatched:', bool(info.PdbUnmatched), 'publics:', bool(info.Publics))

functions = export['functions']
random.seed(1)
samples = random.sample(functions, 2000)
hits = 0
misses = []
for va, size, name in samples:
    probe = va + (size // 2 if size > 1 else 0)
    si = SYMBOL_INFO(SizeOfStruct=88, MaxNameLen=1024)
    disp = ctypes.c_uint64(0)
    if dbghelp.SymFromAddr(process, probe, ctypes.byref(disp), ctypes.byref(si)) and si.Address == va:
        hits += 1
    elif len(misses) < 5:
        misses.append((hex(va), name, hex(si.Address), si.Name.decode('utf-8', 'replace')))

print(f'{hits} / {len(samples)} probes inside functions resolved to the right function')
for m in misses:
    print('  miss', m)
for va, size, name in samples[:4]:
    si = SYMBOL_INFO(SizeOfStruct=88, MaxNameLen=1024)
    disp = ctypes.c_uint64(0)
    dbghelp.SymFromAddr(process, va + 4, ctypes.byref(disp), ctypes.byref(si))
    print(f'  {va + 4:#x} -> {si.Name.decode("utf-8", "replace")} +{disp.value:#x}')

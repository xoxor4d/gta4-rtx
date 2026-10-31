"""
IDA script (File > Script file...): writes every function of the open database as [va, size, name] to
gtaiv_functions.json next to the database. C++ names are demangled to their short form.
The output is the input of make_pdb.py.
"""
import json
import os

import ida_funcs
import ida_name
import idaapi
import idautils
import idc


def export_functions(out_path):
    short_dn = idc.get_inf_attr(idc.INF_SHORT_DN)
    functions = []
    for ea in idautils.Functions():
        func = ida_funcs.get_func(ea)
        raw = ida_name.get_name(ea)
        name = (idc.demangle_name(raw, short_dn) if raw else None) or raw or ("sub_%X" % ea)
        functions.append([ea, func.end_ea - func.start_ea, name])

    with open(out_path, "w", encoding="utf-8") as f:
        json.dump({"imagebase": idaapi.get_imagebase(), "functions": functions}, f)
    return len(functions)


out_path = os.path.join(os.path.dirname(idc.get_idb_path()), "gtaiv_functions.json")
print("exported %d functions to %s" % (export_functions(out_path), out_path))

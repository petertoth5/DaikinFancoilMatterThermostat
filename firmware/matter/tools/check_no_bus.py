#!/usr/bin/env python3
"""Proves that the Matter image (firmware/matter, phase 2a) contains no bus code (task 015).

  check_no_bus.py --self-test
  check_no_bus.py --nm <nm> <elf>      no defined or undefined symbol with a bus prefix
  check_no_bus.py --sources <root>     phase-1 source scan (check_bus_access.scan_tree) on <root>

Symbol check: a symbol is forbidden if, after stripping leading underscores, it starts with one
of FORBIDDEN_PREFIXES. Limit: C++-mangled names (_ZN...) are not matched; bus code is C.

Source scan: loads firmware/tools/check_bus_access.py read-only through importlib and calls its
scan_tree(root). That script has no root argument and never scans firmware/matter/, so this is
the only way to reuse it unmodified. Limits: it relies on an internal function of that script;
components reused from phase 1 are covered by the phase-1 lint, not here; scan_tree looks only
at <root>/main and <root>/components.
"""
import argparse
import importlib.util
import os
import re
import subprocess
import sys

FORBIDDEN_PREFIXES = ("mb_link_", "rs485_port_", "mb_rtu_", "bus_task_", "mbc_", "mbm_", "mbs_",
                      "write_guard_", "bus_write_")
SANITY_SYMBOL = "app_main"  # defined in every image of this project
UNDEFINED_TYPES = ("U", "w", "v")
# nm lines: "<value> <type> <name>"; the value is blank for undefined symbols.
RE_NM_LINE = re.compile(r"^\s*(?:[0-9a-fA-F]+\s+)?([A-Za-z?-])\s+(\S+)\s*$")
BUS_ACCESS_SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                 "..", "..", "tools", "check_bus_access.py")


def parse_nm(nm_output):
    """Return (all symbol names, defined symbol names) from `nm` output."""
    names, defined = set(), set()
    for line in nm_output.splitlines():
        m = RE_NM_LINE.match(line)
        if not m:
            continue
        names.add(m.group(2))
        if m.group(1) not in UNDEFINED_TYPES:
            defined.add(m.group(2))
    return names, defined


def forbidden_symbols(names):
    return sorted(n for n in names if n.lstrip("_").startswith(FORBIDDEN_PREFIXES))


def sanity_problems(defined):
    """An empty or foreign nm output proves nothing, so it fails instead of passing."""
    if not defined:
        return ["no defined symbols parsed from nm output"]
    if SANITY_SYMBOL not in defined:
        return [f"{SANITY_SYMBOL} not defined (wrong ELF or nm output format)"]
    return []


def check_nm_output(nm_output):
    """Return the list of problem messages; empty means the image is clean."""
    names, defined = parse_nm(nm_output)
    problems = sanity_problems(defined)
    if problems:
        return problems
    return [f"forbidden bus symbol: {n}" for n in forbidden_symbols(names)]


def run_nm(nm, elf):
    """Return (nm stdout, error text); error is None on success."""
    if not os.path.isfile(elf):
        return None, f"ELF not found: {elf}"
    try:
        result = subprocess.run([nm, elf], capture_output=True, text=True, check=False)
    except OSError as e:
        return None, f"cannot run nm '{nm}': {e}"
    if result.returncode != 0:
        return None, f"nm exited with {result.returncode}: {result.stderr.strip()}"
    return result.stdout, None


def check_elf(nm, elf):
    nm_output, error = run_nm(nm, elf)
    problems = [error] if error else check_nm_output(nm_output)
    for problem in problems:
        print(f"check_no_bus: FAIL: {problem}")
    if not problems:
        print("check_no_bus: ok: no bus symbols in the image")
    return 1 if problems else 0


def load_scan_tree():
    spec = importlib.util.spec_from_file_location("check_bus_access", BUS_ACCESS_SCRIPT)
    module = importlib.util.module_from_spec(spec)
    sys.dont_write_bytecode = True  # keep firmware/tools free of __pycache__ from this import
    spec.loader.exec_module(module)
    return module.scan_tree


def check_sources(root):
    if not os.path.isdir(root):
        print(f"check_no_bus: FAIL: source root not found: {root}")
        return 1
    violations = load_scan_tree()(root)
    for rel, line, rule, msg in violations:
        print(f"{rel}:{line}: {rule}: {msg}")
    if not violations:
        print("check_no_bus: ok: source scan found no bus-access violations")
    return 1 if violations else 0


def _nm_text(*entries):
    return "\n".join(entries) + "\n"


def _self_test_cases():
    """(name, nm output, whether the check must fail)."""
    clean = ("400d0000 T app_main", "3fc90000 D some_data")
    cases = [("clean image", _nm_text(*clean), False)]
    for prefix in FORBIDDEN_PREFIXES:
        cases.append((f"{prefix} defined", _nm_text(*clean, f"400d0010 T {prefix}x"), True))
        cases.append((f"{prefix} undefined", _nm_text(*clean, f"         U {prefix}x"), True))
        cases.append((f"_{prefix} leading underscore", _nm_text(*clean, f"400d0010 t _{prefix}x"), True))
        cases.append((f"__{prefix} two underscores", _nm_text(*clean, f"400d0010 t __{prefix}x"), True))
    cases += [
        ("near miss xmb_link_a", _nm_text(*clean, "400d0010 T xmb_link_a"), False),
        ("near miss mb_linkx", _nm_text(*clean, "400d0010 T mb_linkx"), False),
        ("near miss mbcx", _nm_text(*clean, "400d0010 T mbcx"), False),
        ("near miss rs485_portx", _nm_text(*clean, "400d0010 T rs485_portx"), False),
        ("weak undefined near miss", _nm_text(*clean, "         w mb_rtux"), False),
        ("empty nm output", "", True),
        ("only blank lines", "\n\n", True),
        ("app_main missing", _nm_text("3fc90000 D some_data"), True),
        ("app_main only undefined", _nm_text("         U app_main"), True),
    ]
    return cases


def self_test():
    failures = 0
    cases = _self_test_cases()
    for name, nm_output, expect_failure in cases:
        failed = bool(check_nm_output(nm_output))
        if failed != expect_failure:
            print(f"self-test FAIL: {name}: expected failure={expect_failure}, got {failed}")
            failures += 1
    if failures:
        print(f"self-test: {failures} of {len(cases)} cases failed")
        return 1
    print(f"self-test: {len(cases)} cases ok")
    return 0


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--nm", metavar="NM", help="nm executable; the ELF is the positional argument")
    parser.add_argument("--sources", metavar="ROOT")
    parser.add_argument("elf", nargs="?")
    args = parser.parse_args(argv)
    if args.self_test:
        return self_test()
    if args.nm:
        if not args.elf:
            parser.error("--nm needs an ELF file")
        return check_elf(args.nm, args.elf)
    if args.sources:
        return check_sources(args.sources)
    parser.error("one of --self-test, --nm, --sources is required")


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

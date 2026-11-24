#!/usr/bin/env python3
"""ELF symbol check (task 013, architecture section 3, D20).

Fails the build if the linked image defines an esp-modbus master/slave symbol or a
mb_link_write* symbol, or lacks a symbol that is required to be present. Fails (never
skips) if nm or the ELF is unusable, or if the nm output looks wrong (no defined symbols, or
no app_main, which every image of this project defines).

  check_elf_symbols.py --nm <nm> [--require SYM[,SYM...]] <elf>
  check_elf_symbols.py --self-test
"""
import argparse
import contextlib
import io
import os
import re
import subprocess
import sys
import tempfile

# mbc_/mbm_ esp-modbus master/controller, mbs_ slave, mb_link_write* the phase-3 write path.
RE_FORBIDDEN = re.compile(r"^(?:mbc_|mbm_|mbs_|mb_link_write)")
# nm --defined-only lines: "<value> <type> <name>"; value is blank for undefined symbols
# (not listed with --defined-only, but tolerated) and absolute symbols have type A.
RE_NM_LINE = re.compile(r"^\s*(?:[0-9a-fA-F]+\s+)?([A-Za-z?])\s+(\S+)\s*$")
UNDEFINED_TYPES = ("U", "w", "v")
SANITY_SYMBOL = "app_main"  # defined by the main component in every image


def defined_symbols(nm_output):
    """Names of defined symbols in `nm` output; undefined (U/w/v) entries are dropped."""
    names = set()
    for line in nm_output.splitlines():
        m = RE_NM_LINE.match(line)
        if m and m.group(1) not in UNDEFINED_TYPES:
            names.add(m.group(2))
    return names


def sanity_problems(nm_output):
    """Messages if the nm output cannot be trusted (an empty or foreign output proves nothing)."""
    defined = defined_symbols(nm_output)
    if not defined:
        return ["no defined symbols parsed from nm output"]
    if SANITY_SYMBOL not in defined:
        return [f"{SANITY_SYMBOL} not defined (wrong ELF or nm output format)"]
    return []


def evaluate(nm_output, required):
    """Return (forbidden symbols sorted, missing required symbols in given order)."""
    defined = defined_symbols(nm_output)
    forbidden = sorted(name for name in defined if RE_FORBIDDEN.match(name))
    missing = [name for name in required if name not in defined]
    return forbidden, missing


def run_nm(nm, elf):
    """Return (nm stdout, error text); error is None on success."""
    if not os.path.isfile(elf):
        return None, f"ELF not found: {elf}"
    try:
        result = subprocess.run([nm, "--defined-only", elf], capture_output=True,
                                text=True, check=False)
    except OSError as e:
        return None, f"cannot run nm '{nm}': {e}"
    if result.returncode != 0:
        return None, f"nm exited with {result.returncode}: {result.stderr.strip()}"
    return result.stdout, None


def parse_required(value):
    return [name for name in value.split(",") if name]


def check(nm, elf, required):
    nm_output, error = run_nm(nm, elf)
    if error:
        print(f"check_elf_symbols: FAIL: {error}")
        return 1
    problems = sanity_problems(nm_output)
    for problem in problems:
        print(f"check_elf_symbols: FAIL: {problem}")
    if problems:
        return 1
    forbidden, missing = evaluate(nm_output, required)
    for name in forbidden:
        print(f"{elf}: forbidden symbol defined: {name}")
    for name in missing:
        print(f"{elf}: required symbol missing: {name}")
    if forbidden or missing:
        return 1
    required_text = f"{','.join(required)} present" if required else "none configured"
    print(f"check_elf_symbols: ok: forbidden 0 found; required: {required_text}")
    return 0


def _nm_lines(*entries):
    return "\n".join(entries) + "\n"


def _evaluate_cases():
    """(name, nm output, required, expected forbidden, expected missing)."""
    plain = _nm_lines("400d0000 T app_main", "3fc90000 D some_data")
    return (
        ("clean image", plain, [], [], []),
        ("forbidden mbc_ defined", _nm_lines("400d0000 T mbc_dummy"), [], ["mbc_dummy"], []),
        ("forbidden mbm_ defined", _nm_lines("400d0000 t mbm_rq_x"), [], ["mbm_rq_x"], []),
        ("forbidden mbs_ defined", _nm_lines("3fc90000 D mbs_state"), [], ["mbs_state"], []),
        ("forbidden mb_link_write defined", _nm_lines("400d0000 T mb_link_write"), [],
         ["mb_link_write"], []),
        ("prefix mb_link_writer_x", _nm_lines("400d0000 T mb_link_writer_x"), [],
         ["mb_link_writer_x"], []),
        ("absolute symbol", _nm_lines("00000000 A mbc_abs"), [], ["mbc_abs"], []),
        ("undefined only", _nm_lines("         U mbc_master_start", "         w mbm_x"), [], [], []),
        ("anchored xmbc_foo", _nm_lines("400d0000 T xmbc_foo", "400d0004 T mb_link_read"),
         [], [], []),
        ("mb_link_read is not forbidden", _nm_lines("400d0000 T mb_link_read"), [], [], []),
        ("required present", _nm_lines("400d0000 T mb_link_read", "400d0010 T mb_rtu_build_fc03"),
         ["mb_link_read", "mb_rtu_build_fc03"], [], []),
        ("required missing", plain, ["mb_link_read"], [], ["mb_link_read"]),
        ("required only undefined", _nm_lines("         U mb_link_read"), ["mb_link_read"],
         [], ["mb_link_read"]),
        ("required one of two missing", _nm_lines("400d0000 T mb_link_read"),
         ["mb_link_read", "mb_rtu_build_fc03"], [], ["mb_rtu_build_fc03"]),
        ("required substring only", _nm_lines("400d0000 T mb_link_read_x"), ["mb_link_read"],
         [], ["mb_link_read"]),
        ("forbidden and missing", _nm_lines("400d0000 T mbc_x"), ["dmt_y"], ["mbc_x"], ["dmt_y"]),
    )


def _parse_required_cases():
    return (("", []), ("a", ["a"]), ("a,b", ["a", "b"]), ("a,,b,", ["a", "b"]))


def _failure_mode_cases(scratch_file):
    """(name, nm, elf) for the fail-never-skip paths; each must make check() return 1."""
    return (
        ("missing nm", "/nonexistent/nm", scratch_file),
        ("non-executable nm", scratch_file, scratch_file),
        ("missing ELF", sys.executable, "/nonexistent/image.elf"),
        ("nm exits non-zero", sys.executable, scratch_file),  # python rejects '--defined-only'
    )


def _sanity_cases():
    """(name, nm output, expected number of problems)."""
    return (
        ("sanity empty output", "", 1),
        ("sanity undefined only", _nm_lines("         U app_main"), 1),
        ("sanity unparseable lines", _nm_lines("foo.o:", ""), 1),
        ("sanity app_main missing", _nm_lines("400d0000 T other"), 1),
        ("sanity app_main present", _nm_lines("400d0000 T app_main"), 0),
    )


def _stub_nm_cases():
    """(name, stub output, expected check() exit code); the control proves the stub mechanism."""
    return (
        ("stub nm prints nothing", "", 1),
        ("stub nm without app_main", "400d0000 T other\n", 1),
        ("stub nm control with app_main", "400d0000 T app_main\n", 0),
    )


def _stub_nm_test():
    failures, total = 0, 0
    with tempfile.TemporaryDirectory() as scratch:
        elf = os.path.join(scratch, "image.elf")
        with open(elf, "w", encoding="utf-8") as f:
            f.write("stub elf\n")
        for name, output, want in _stub_nm_cases():
            total += 1
            stub = os.path.join(scratch, "stub_nm.sh")
            with open(stub, "w", encoding="utf-8") as f:
                f.write(f"#!/bin/sh\ncat <<'EOF'\n{output}EOF\n")  # ignores its arguments
            os.chmod(stub, 0o755)
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                exit_code = check(stub, elf, [])
            if exit_code != want:
                failures += 1
                print(f"self-test FAIL {name}: expected exit {want}, got {exit_code}")
    return failures, total


def _failure_modes_test():
    failures, total = 0, 0
    with tempfile.TemporaryDirectory() as scratch:
        plain_file = os.path.join(scratch, "plain.txt")
        with open(plain_file, "w", encoding="utf-8") as f:
            f.write("not an executable\n")
        for name, nm, elf in _failure_mode_cases(plain_file):
            total += 1
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                exit_code = check(nm, elf, [])
            if exit_code != 1:
                failures += 1
                print(f"self-test FAIL {name}: expected exit 1, got {exit_code}")
    return failures, total


def self_test():
    failures, total = 0, 0
    for name, output, required, want_forbidden, want_missing in _evaluate_cases():
        total += 1
        got = evaluate(output, required)
        if got != (want_forbidden, want_missing):
            failures += 1
            print(f"self-test FAIL {name}: expected {(want_forbidden, want_missing)}, got {got}")
    for text, want in _parse_required_cases():
        total += 1
        if parse_required(text) != want:
            failures += 1
            print(f"self-test FAIL parse_required {text!r}")
    for name, output, want in _sanity_cases():
        total += 1
        got = len(sanity_problems(output))
        if got != want:
            failures += 1
            print(f"self-test FAIL {name}: expected {want} problems, got {got}")
    for test in (_stub_nm_test, _failure_modes_test):
        test_failures, test_total = test()
        failures += test_failures
        total += test_total
    if failures:
        print(f"self-test: {failures} of {total} cases failed")
        return 1
    print(f"self-test: {total} cases ok")
    return 0


def main(argv):
    if argv[1:] == ["--self-test"]:
        return self_test()
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--nm", required=True)
    parser.add_argument("--require", default="")
    parser.add_argument("elf")
    args = parser.parse_args(argv[1:])
    return check(args.nm, args.elf, parse_required(args.require))


if __name__ == "__main__":
    sys.exit(main(sys.argv))

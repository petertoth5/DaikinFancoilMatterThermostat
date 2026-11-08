#!/usr/bin/env python3
"""Bus-access lint (rules R1-R8, task 001, architecture section 3).

Scans *.c *.h *.cpp *.cc *.cxx *.hpp *.hh *.S under main/ and components/ (paths
relative to firmware/). Comments and strings are not exempt. Backslash-newline
continuations are spliced before matching. Output: path:line: R<n>: message, exit 1.

Out of scope: `#include MACRO`, `##` token pasting, positional struct initializers
and aliases for R8. R8 is a heuristic; the real guard is that the FC06 constant
lives only in WRITE_FILE, plus the ELF check in task 002.

  check_bus_access.py              scan the tree
  check_bus_access.py --self-test  run the built-in virtual-path cases
"""
import os
import re
import sys
import tempfile

SCAN_ROOTS = ("main", "components")
SCAN_SUFFIXES = (".c", ".h", ".cpp", ".cc", ".cxx", ".hpp", ".hh", ".S")
SKIP_DIRS = ("managed_components", "test_host")
RE_BUILD_DIR = re.compile(r"build(?:[-_].*)?")

WRITE_FILE = "components/mb_link/src/mb_link_write.c"
WRITE_HEADER = "components/mb_link/include/mb_link_write.h"
MB_LINK_SRC = "components/mb_link/src/"
RS485_PORT = "components/rs485_port/"

R1_ALLOWED_CALLS = {
    "mbc_master_create_serial",
    "mbc_master_start",
    "mbc_master_stop",
    "mbc_master_delete",
    "mbc_master_send_request",
}
UART_MODE_ALLOWED_ARGS = ("UART_MODE_RS485_HALF_DUPLEX", "UART_MODE_UART")
READ_COMMANDS = ("0x03", "MB_FUNC_READ_HOLDING_REGISTER")

RE_MASTER = re.compile(r"\bmbc_master_\w*")
RE_SLAVE = re.compile(r"\b(?:mbc_slave_\w*|mbs_\w*|mbc_tcp_\w*)")
RE_INCLUDE = re.compile(r"#\s*include\s*[<\"]([^>\"]*)[>\"]")
RE_UART_TX = re.compile(
    r"\b(?:uart_write_bytes\w*|uart_tx_chars|uart_set_rts|uart_set_dtr"
    r"|uart_set_loop_back|uart_set_line_inverse|_uart_set_pin\w*)\b")
RE_UART_SETUP = re.compile(
    r"\b(?:uart_set_pin|uart_set_mode|uart_driver_install|uart_driver_delete"
    r"|uart_param_config|uart_read_bytes)\b")
RE_UART_SET_MODE = re.compile(r"\buart_set_mode\s*\(")
RE_GPIO_CFG = re.compile(r"CONFIG_DMT_BUS_(?:TX|DE)_GPIO")
RE_LINK_WRITE = re.compile(r"\bmb_link_write\w*")
RE_FUNC_WRITE = re.compile(r"\bMB_FUNC_WRITE\w*")
RE_COMMAND_ASSIGN = re.compile(r"\bcommand\s*([-+*/%&|^]|<<|>>)?=(?!=)([^;,}]*)")
# GCC also accepts trailing blanks after the backslash (with a warning) and CRLF.
RE_CONTINUATION = re.compile(r"\\[ \t]*\r?\n")


def _line_of(text, offset):
    return text.count("\n", 0, offset) + 1


def _second_argument(text, open_paren_end):
    """Second top-level argument of a call; text after the '(' up to its ')'."""
    depth, args, current = 0, [], []
    for ch in text[open_paren_end:]:
        if ch == "(":
            depth += 1
        elif ch == ")" and depth == 0:
            break
        elif ch == ")":
            depth -= 1
        if ch == "," and depth == 0:
            args.append("".join(current))
            current = []
        else:
            current.append(ch)
    args.append("".join(current))
    return args[1].strip() if len(args) > 1 else ""


def _check_r1(path, text):
    for m in RE_MASTER.finditer(text):
        if not path.startswith(MB_LINK_SRC):
            yield m.start(), "R1", f"{m.group()} only allowed in {MB_LINK_SRC}"
        elif m.group() not in R1_ALLOWED_CALLS:
            yield m.start(), "R1", f"{m.group()} is not an allowed esp-modbus master call"


def _check_r2(path, text):
    for m in RE_SLAVE.finditer(text):
        yield m.start(), "R2", f"{m.group()} (esp-modbus slave/TCP) is banned"
    for m in RE_INCLUDE.finditer(text):
        if os.path.basename(m.group(1)) == "esp_modbus_slave.h":
            yield m.start(), "R2", "esp_modbus_slave.h include is banned"


def _check_r3(path, text):
    if path.startswith(MB_LINK_SRC):
        return
    for m in RE_INCLUDE.finditer(text):
        if os.path.basename(m.group(1)) in ("esp_modbus_master.h", "esp_modbus_common.h"):
            yield m.start(), "R3", f"{m.group(1)} include only allowed in {MB_LINK_SRC}"


def _check_r4(path, text):
    for m in RE_UART_TX.finditer(text):
        yield m.start(), "R4", f"{m.group()} is banned (UART TX/RTS/DTR/loopback/invert)"


def _check_r5(path, text):
    if not path.startswith(RS485_PORT):
        for m in RE_UART_SETUP.finditer(text):
            yield m.start(), "R5", f"{m.group()} only allowed in {RS485_PORT}"
    for m in RE_UART_SET_MODE.finditer(text):
        mode = _second_argument(text, m.end())
        if mode not in UART_MODE_ALLOWED_ARGS:
            yield m.start(), "R5", f"uart_set_mode mode must be one of {UART_MODE_ALLOWED_ARGS}, got '{mode}'"


def _check_r6(path, text):
    if path.startswith(RS485_PORT) or path.startswith("main/"):
        return
    for m in RE_GPIO_CFG.finditer(text):
        yield m.start(), "R6", f"{m.group()} only allowed in {RS485_PORT} and main/"


def _check_r7(path, text):
    if path in (WRITE_FILE, WRITE_HEADER):
        return
    for m in RE_LINK_WRITE.finditer(text):
        yield m.start(), "R7", f"{m.group()} only allowed in {WRITE_FILE} and {WRITE_HEADER}"


def _check_r8(path, text):
    if not path.startswith("components/") or path == WRITE_FILE:
        return
    for m in RE_FUNC_WRITE.finditer(text):
        yield m.start(), "R8", f"{m.group()} only allowed in {WRITE_FILE}"
    if not path.startswith("components/mb_link/"):
        return
    for m in RE_COMMAND_ASSIGN.finditer(text):
        operator, value = m.group(1), m.group(2).strip()
        if operator:
            yield m.start(), "R8", f"compound assignment '{operator}=' to command is banned"
        elif value not in READ_COMMANDS:
            yield m.start(), "R8", f"command must be one of {READ_COMMANDS}, got '{value}'"


CHECKS = (_check_r1, _check_r2, _check_r3, _check_r4,
          _check_r5, _check_r6, _check_r7, _check_r8)


def _splice_continuations(text):
    """Join backslash-newline lines; also return spliced offsets of each removed newline."""
    removed_at, pieces, last, removed_chars = [], [], 0, 0
    for m in RE_CONTINUATION.finditer(text):
        pieces.append(text[last:m.start()])
        removed_at.append(m.start() - removed_chars)
        removed_chars += m.end() - m.start()
        last = m.end()
    pieces.append(text[last:])
    return "".join(pieces), removed_at


def check_source(path, text):
    """Return sorted (line, rule, message) violations for one file."""
    spliced, removed_at = _splice_continuations(text)
    found = set()
    for check in CHECKS:
        for offset, rule, message in check(path, spliced):
            hidden = sum(1 for pos in removed_at if pos <= offset)
            found.add((_line_of(spliced, offset) + hidden, rule, message))
    return sorted(found)


def _iter_sources(root):
    for top in SCAN_ROOTS:
        for dirpath, dirnames, filenames in os.walk(os.path.join(root, top)):
            dirnames[:] = sorted(d for d in dirnames
                                 if d not in SKIP_DIRS and not RE_BUILD_DIR.fullmatch(d))
            for name in sorted(filenames):
                if name.endswith(SCAN_SUFFIXES):
                    yield os.path.join(dirpath, name)


def scan_tree(root):
    violations = []
    for full in _iter_sources(root):
        rel = os.path.relpath(full, root).replace(os.sep, "/")
        with open(full, encoding="utf-8", errors="replace") as f:
            text = f.read()
        violations += [(rel, line, rule, msg) for line, rule, msg in check_source(rel, text)]
    return violations


# (virtual path, snippet, expected set of rules); empty set = must pass.
SELF_TEST_CASES = (
    # R1
    ("components/mb_link/src/mb_link.c", "mbc_master_send_request(h, &r, d);", set()),
    ("components/mb_link/src/mb_link.c", "mbc_master_create_serial(&c, &h);", set()),
    ("components/mb_link/src/mb_link.c", "mbc_master_start(h); mbc_master_stop(h); mbc_master_delete(h);", set()),
    ("components/mb_link/src/mb_link.c", "mbc_master_set_parameter(h, 1, 2, 3);", {"R1"}),
    ("components/mb_link/include/mb_link.h", "mbc_master_start(h);", {"R1"}),
    ("main/main.c", "mbc_master_start(h);", {"R1"}),
    # R2
    ("components/mb_link/src/mb_link.c", "mbc_slave_start(h);", {"R2"}),
    ("main/main.c", "mbs_init();", {"R2"}),
    ("main/main.c", "mbc_tcp_master_x();", {"R2"}),
    ("components/mb_link/src/mb_link.c", '#include "esp_modbus_slave.h"', {"R2"}),
    # R3
    ("components/mb_link/src/mb_link.c", '#include "esp_modbus_master.h"', set()),
    ("components/mb_link/src/mb_link.c", "#include <esp_modbus_common.h>", set()),
    ("components/mb_link/include/mb_link.h", '#include "esp_modbus_master.h"', {"R3"}),
    ("main/main.c", "#include \"esp_modbus_common.h\"", {"R3"}),
    # R4
    ("main/main.c", 'uart_write_bytes(1, "x", 1);', {"R4"}),
    ("components/rs485_port/src/rs485_port.c", "uart_tx_chars(1, b, 1);", {"R4"}),
    ("components/mb_link/src/mb_link.c", "uart_set_rts(1, 0);", {"R4"}),
    ("main/main.c", "uart_set_dtr(1, 0);", {"R4"}),
    ("main/main.c", "uart_set_loop_back(1, true);", {"R4"}),
    ("main/main.c", "uart_set_line_inverse(1, 0);", {"R4"}),
    ("components/rs485_port/src/rs485_port.c", "_uart_set_pin6(1);", {"R4"}),
    ("main/main.c", "uart_write_bytes_with_break(1, b, 1, 2);", {"R4"}),
    # R5
    ("components/rs485_port/src/rs485_port.c", "uart_set_mode(p, UART_MODE_RS485_HALF_DUPLEX);", set()),
    ("components/rs485_port/src/rs485_port.c", "uart_set_mode(p,\n    UART_MODE_UART);", set()),
    ("components/rs485_port/src/rs485_port.c", "uart_driver_install(p, 256, 0, 0, NULL, 0); uart_read_bytes(p, b, 1, 1);", set()),
    ("components/rs485_port/src/rs485_port.c", "uart_set_mode(p, UART_MODE_RS485_APP_CTRL);", {"R5"}),
    ("components/rs485_port/src/rs485_port.c", "uart_set_mode(p, mode_var);", {"R5"}),
    ("components/mb_link/src/mb_link.c", "uart_set_mode(p, UART_MODE_UART);", {"R5"}),
    ("main/main.c", "uart_driver_install(1, 256, 0, 0, NULL, 0);", {"R5"}),
    ("main/main.c", "uart_set_pin(1, 5, 4, 6, -1);", {"R5"}),
    ("components/mb_link/src/mb_link.c", "uart_param_config(1, &cfg);", {"R5"}),
    ("main/main.c", "uart_read_bytes(1, b, 1, 1);", {"R5"}),
    ("main/main.c", "uart_driver_delete(1);", {"R5"}),
    # R6
    ("main/main.c", "int g = CONFIG_DMT_BUS_TX_GPIO;", set()),
    ("components/rs485_port/src/rs485_port.c", "int g = CONFIG_DMT_BUS_DE_GPIO;", set()),
    ("components/mb_link/src/mb_link.c", "int g = CONFIG_DMT_BUS_DE_GPIO;", {"R6"}),
    ("components/bus_task/src/x.c", "int g = CONFIG_DMT_BUS_TX_GPIO;", {"R6"}),
    ("components/bus_task/src/x.c", "int g = CONFIG_DMT_BUS_RX_GPIO;", set()),
    # R7
    ("components/mb_link/src/mb_link_write.c", "int mb_link_write(int a);", set()),
    ("components/mb_link/include/mb_link_write.h", "int mb_link_write(int a);", set()),
    ("components/mb_link/src/mb_link.c", "mb_link_write(1, 2, 3);", {"R7"}),
    ("main/main.c", "mb_link_write_reg(1);", {"R7"}),
    # R8
    ("components/mb_link/src/mb_link_write.c", "r.command = 0x06; x = MB_FUNC_WRITE_SINGLE_REGISTER;", set()),
    ("components/mb_link/src/mb_link.c", "r.command = 0x03;", set()),
    ("components/mb_link/src/mb_link.c", "{ .command = MB_FUNC_READ_HOLDING_REGISTER, .x = 1 }", set()),
    ("components/mb_link/src/mb_link.c", "if (r.command == 0x06) {}", set()),
    ("components/mb_link/src/mb_link.c", "r.command = 0x06;", {"R8"}),
    ("components/mb_link/src/mb_link.c", "command = fc;", {"R8"}),
    ("components/mb_link/src/mb_link.c", "r.command =\n    0x10;", {"R8"}),
    ("components/mb_link/src/mb_link.c", "x = MB_FUNC_WRITE_SINGLE_REGISTER;", {"R8"}),
    ("components/rs485_port/src/rs485_port.c", "x = MB_FUNC_WRITE_MULTIPLE_REGISTERS;", {"R8"}),
    ("main/main.c", "x = MB_FUNC_WRITE_SINGLE_REGISTER;", set()),
    ("components/mb_link/src/mb_link.c", "r.command |= 0x04;", {"R8"}),
    ("components/mb_link/src/mb_link.c", "r.command <<= 1;", {"R8"}),
    ("components/mb_link/src/mb_link.c", "r.command |= 0x03;", {"R8"}),
    ("components/mb_link/src/mb_link.c", "r.command &= 0x03;", {"R8"}),
    ("components/mb_link/src/mb_link.c", "r.command += 0x03;", {"R8"}),
    ("components/mb_link/src/mb_link.c", "if (r.command >= 0x06 || r.command != 0x06) {}", set()),
    # comments, strings, continuations, multi-line calls
    ("main/main.c", "// uart_write_bytes(1, b, 1);", {"R4"}),
    ("main/main.c", 'puts("uart_write_bytes");', {"R4"}),
    ("main/main.c", "uart_\\\nwrite_bytes(1, b, 1);", {"R4"}),
    ("components/rs485_port/src/rs485_port.c", "uart_set_mode(p,\n    UART_MODE_RS485_APP_CTRL);", {"R5"}),
    ("main/main.c", "uart_\\\r\nwrite_bytes(1);", {"R4"}),
    ("main/main.c", "uart_\\ \t\nwrite_bytes(1);", {"R4"}),
)

# (snippet, expected line of the single violation): offsets must survive splices.
LINE_NUMBER_CASES = (
    ("int a;\nuart_\\\r\nwrite_bytes(1);\n", 2),
    ("int a;\nuart_\\ \t\nwrite_bytes(1);\n", 2),
    ("a = \\\r\n1;\nuart_write_bytes(1);\n", 3),
)


def _tree_test_expectations():
    """(relative file, scanned?) cases for the tree walker."""
    return (
        ("components/c/test_host/a.c", False),
        ("components/c/test_x/a.c", True),
        ("components/c/src/test/a.c", True),
        ("main/test/a.c", True),
        ("components/buildinfo/a.c", True),
        ("components/c/build/a.c", False),
        ("components/c/build-verify/a.c", False),
        ("components/c/build_x/a.c", False),
        ("components/c/managed_components/a.c", False),
        ("components/c/a.hpp", True),
        ("components/c/a.cc", True),
        ("components/c/a.cxx", True),
        ("components/c/a.hh", True),
        ("components/c/a.S", True),
        ("components/c/a.txt", False),
    )


def _tree_walker_test():
    failures = 0
    with tempfile.TemporaryDirectory() as root:
        cases = _tree_test_expectations()
        for rel, _ in cases:
            full = os.path.join(root, rel)
            os.makedirs(os.path.dirname(full), exist_ok=True)
            with open(full, "w", encoding="utf-8") as f:
                f.write("uart_write_bytes(1, b, 1);\n")  # R4 marker if scanned
        flagged = {rel for rel, _, _, _ in scan_tree(root)}
        for rel, scanned in cases:
            if (rel in flagged) != scanned:
                failures += 1
                print(f"self-test FAIL tree walker {rel}: scanned={rel in flagged}, expected {scanned}")
    return failures, len(cases)


def self_test():
    failures = 0
    for path, snippet, expected in SELF_TEST_CASES:
        got = {rule for _, rule, _ in check_source(path, snippet)}
        if got != expected:
            failures += 1
            print(f"self-test FAIL {path}: expected {sorted(expected)}, got {sorted(got)}: {snippet!r}")
    for snippet, line in LINE_NUMBER_CASES:
        got = [l for l, _, _ in check_source("main/main.c", snippet)]
        if got != [line]:
            failures += 1
            print(f"self-test FAIL line numbers: expected [{line}], got {got}: {snippet!r}")
    tree_failures, tree_cases = _tree_walker_test()
    failures += tree_failures
    total = len(SELF_TEST_CASES) + len(LINE_NUMBER_CASES) + tree_cases
    if failures:
        print(f"self-test: {failures} of {total} cases failed")
        return 1
    print(f"self-test: {total} cases ok")
    return 0


def main(argv):
    if argv[1:] == ["--self-test"]:
        return self_test()
    if len(argv) > 1:
        print(__doc__)
        return 2
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    violations = scan_tree(root)
    for rel, line, rule, msg in violations:
        print(f"{rel}:{line}: {rule}: {msg}")
    return 1 if violations else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

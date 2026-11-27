#!/usr/bin/env python3
"""Bus-access lint (rules R1-R9, tasks 001, 013 and 019a, architecture section 3).

Scans *.c *.h *.cpp *.cc *.cxx *.hpp *.hh *.S and files named exactly idf_component.yml
(only R3 applies to those) under main/ and components/ (paths relative to firmware/).
Comments and strings are not exempt. Backslash-newline continuations are spliced before
matching (not in manifests). Output: path:line: R<n>: message, exit 1.

  R1  no esp-modbus master/controller API (mbc_*, mbm_*) anywhere
  R2  no esp-modbus slave/TCP API, no esp_modbus_slave.h
  R3  no esp-modbus header (esp_modbus_*.h, mbcontroller.h) and no esp-modbus in a manifest
  R4  UART TX/RTS/DTR/loopback/invert calls banned; only the exact uart_write_bytes in
      components/rs485_port/src/. Banned everywhere (also in rs485_port/src): uart_ll_*,
      uart_hal_*, uart_tx_one_char, esp_rom_output_*, esp_rom_uart_*, esp_rom_install_*, raw
      UART0./UART1./UART2. register access (also `->`), REG_WRITE/WRITE_PERI_REG/REG_READ/
      READ_PERI_REG with a UART_FIFO*_REG argument, and the uart_struct.h, uart_ll.h,
      uart_hal.h, uart_reg.h includes. Other esp_rom_* calls (esp_rom_delay_us) are allowed
  R5  UART setup (driver install/config/pin/mode/baud/parity/flow control/ISR/flush ...) only
      in components/rs485_port/, uart_set_mode only with allowed modes
  R6  CONFIG_DMT_BUS_TX/DE_GPIO only in components/rs485_port/ and main/
  R7  mb_link_write* only in mb_link_write.c/.h and components/bus_task/src/bus_write.c
  R8  function-code bytes only in mb_rtu: 0x03/0x83 (read) as the two defines in mb_rtu.c,
      0x06/0x86 (write single register) as the two defines in mb_rtu_write.c, nothing else
  R9  FC06 codec names (mb_rtu_build_fc06, mb_rtu_parse_fc06, mb_rtu_fc06_*) only in
      mb_rtu_write.c/.h and mb_link_write.c

Documented limits (not detected): `#include MACRO`, `##` token pasting, decimal, octal and
binary function-code literals, function codes computed at run time or passed through
variables, aliases/pointers to an FC field, array elements. R8 is a heuristic; the real guard
is that FC bytes exist only in mb_rtu plus the ELF symbol check (task 013,
tools/check_elf_symbols.py). Further limits:
  - R8 (d) ++/-- and assignment: casts or dereference in the operand (`++(*p).fc`,
    `++((T *)p)->fc`) are not detected; nested brackets in a prefix operand
    (`++a[b[1]].fc`) are not detected either (the bracket gap `\\[[^\\]]*\\]` does not nest).
  - Arithmetic or bit operations on the FC constants inside components/mb_rtu/src/
    (`MB_RTU_FC_READ_HOLDING + 3`, `MB_RTU_FC_READ_HOLDING | 0x80`) are not detected;
    mb_rtu/src is the trusted place (review and the host tests cover it).
  - Character and string escapes (`'\\x06'`, `'\\006'`, `"\\x06"`) are not detected as FC literals.
  - CMakeLists.txt, *.cmake and Kconfig* files are not scanned (e.g. `-DFC=0x06`).
  - "Exactly one uart_write_bytes call site" cannot be counted here (014 inspection item 3e).
  - Raw UART register access is detected only as UART0./UART1./UART2. (or `->`); `&UART1`,
    `UART_LL_GET_HW(1)` or a `uart_dev_t *` are not, and the uart_struct.h/uart_ll.h/uart_hal.h/
    uart_reg.h include ban does not see transitive includes.
  - UART FIFO register macros (R4): the UART_FIFO*_REG name must stand within 200 characters
    after the `(` of REG_WRITE/WRITE_PERI_REG/REG_READ/READ_PERI_REG and before the next `;`
    (bounded window, keeps matching linear). A longer argument list, or the register reached
    through another macro or a variable, is not detected.
  - Trigraph `??=include` is not detected (GCC ignores trigraphs in GNU modes, inferred).
  - Known false positives (comments and strings are not exempt): a line comment ending in
    `--`/`++` directly before a line starting with an FC name (`// ----` then `fc = ...`) and
    prose such as `... UART1.` in a comment report R8 resp. R4; reword the comment.
  - R5 uart_set_mode argument parsing (_second_argument, task 001 code) is quadratic on many
    repeated `uart_set_mode(` without a closing `)`: each occurrence scans to the end of the file.
Regex hygiene: every gap/comment pattern is built from _CMT, which can only end at the first
`*/`; alternatives inside a repeated group never match the same text, so there is no exponential
backtracking. Matching time is linear in the input for closed comments, but quadratic for an
unterminated `/*` or many repeated `/*` (each one rescans to the end of the input); there is no
cap on the comment length. The self-test times every case (limit CASE_TIME_LIMIT_S).

  check_bus_access.py              scan the tree
  check_bus_access.py --self-test  run the built-in virtual-path cases
"""
import os
import re
import sys
import tempfile
import time

SCAN_ROOTS = ("main", "components")
SCAN_SUFFIXES = (".c", ".h", ".cpp", ".cc", ".cxx", ".hpp", ".hh", ".S")
MANIFEST_NAME = "idf_component.yml"
SKIP_DIRS = ("managed_components", "test_host")
RE_BUILD_DIR = re.compile(r"build(?:[-_].*)?")

WRITE_FILE = "components/mb_link/src/mb_link_write.c"
WRITE_HEADER = "components/mb_link/include/mb_link_write.h"
BUS_WRITE_FILE = "components/bus_task/src/bus_write.c"
MB_RTU_WRITE_HEADER = "components/mb_rtu/include/mb_rtu_write.h"
RS485_PORT = "components/rs485_port/"
RS485_PORT_SRC = "components/rs485_port/src/"
MB_RTU_SRC = "components/mb_rtu/src/"
MB_RTU_C = "components/mb_rtu/src/mb_rtu.c"
MB_RTU_WRITE_C = "components/mb_rtu/src/mb_rtu_write.c"
# R8 (c)/(d): the components where function-code bytes must not appear.
FC_COMPONENTS = ("components/mb_rtu/", "components/mb_link/", RS485_PORT)
# R8 (d): simple assignment to a function-code field is banned only here (mb_rtu parses frames).
FC_SIMPLE_ASSIGN_COMPONENTS = ("components/mb_link/", RS485_PORT)

UART_MODE_ALLOWED_ARGS = ("UART_MODE_RS485_HALF_DUPLEX", "UART_MODE_UART")
UART_WRITE_ALLOWED = "uart_write_bytes"
# The only MB_RTU_FC_* names, per file, with the literal their #define must have
# (Modbus FC03 / exception 0x83 in mb_rtu.c, FC06 / exception 0x86 in mb_rtu_write.c).
FC_DEFINE_LITERALS_BY_FILE = {
    MB_RTU_C: {"MB_RTU_FC_READ_HOLDING": "0x03", "MB_RTU_FC_READ_HOLDING_EXC": "0x83"},
    MB_RTU_WRITE_C: {"MB_RTU_FC_WRITE_SINGLE": "0x06", "MB_RTU_FC_WRITE_SINGLE_EXC": "0x86"},
}
READ_FC_VALUES = (0x03, 0x83)
WRITE_FC_VALUES = (0x05, 0x06, 0x0F, 0x10, 0x15, 0x16, 0x17, 0x86)

RE_MASTER = re.compile(r"\bmbc_(?!slave_|tcp_)\w*|\bmbm_\w*")
RE_SLAVE = re.compile(r"\b(?:mbc_slave_\w*|mbs_\w*|mbc_tcp_\w*)")
# One block comment; it can only end at its first "*/" and its alternatives never overlap, so no
# input can be split into comments in more than one way (no exponential backtracking).
_CMT = r"/\*[^*]*\*+(?:[^/*][^*]*\*+)*/"
# Blanks and block comments between the tokens of a directive (a comment may contain newlines).
_DIR_GAP = rf"(?:[ \t]|{_CMT})*"
# Include directive: '#' or the digraph '%:', include/include_next/import (A1); group 1 = header.
RE_INCLUDE = re.compile(
    rf"(?:#|%:){_DIR_GAP}(?:include(?:_next)?|import){_DIR_GAP}[<\"]([^>\"]*)[>\"]")
RE_MANIFEST_MODBUS = re.compile(r"esp[-_]modbus", re.IGNORECASE)
RE_UART_TX = re.compile(
    r"\b(?:uart_write_bytes\w*|uart_tx_chars|uart_set_rts|uart_set_dtr"
    r"|uart_set_loop_back|uart_set_line_inverse|_uart_set_pin\w*"
    r"|uart_ll_\w*|uart_hal_\w*|uart_tx_one_char"
    # ROM routes to a UART: the whole families, no ROM UART call is needed here
    r"|esp_rom_(?:output|uart|install)_\w*)\b")
RE_UART_REGISTER = re.compile(rf"\bUART[0-2](?:\s|{_CMT})*(?:\.|->)")
# Register macro whose arguments name a UART FIFO register (TX write or RX read around the driver).
# The gap is a bounded window, not `[^;]*`, which is quadratic on repeated `REG_WRITE(`.
FIFO_REG_WINDOW = 200
RE_UART_FIFO_REG_ACCESS = re.compile(
    rf"\b(?:REG_WRITE|WRITE_PERI_REG|REG_READ|READ_PERI_REG)\s*\([^;]{{0,{FIFO_REG_WINDOW}}}"
    r"\bUART_FIFO\w*_REG\b")
UART_LOW_LEVEL_HEADERS = ("uart_struct.h", "uart_ll.h", "uart_hal.h", "uart_reg.h")
RE_UART_SETUP = re.compile(
    r"\b(?:uart_set_pin|uart_set_mode|uart_driver_install|uart_driver_delete"
    r"|uart_param_config|uart_read_bytes|uart_set_hw_flow_ctrl|uart_set_baudrate"
    r"|uart_set_parity|uart_set_word_length|uart_set_stop_bits|uart_set_tx_idle_num"
    r"|uart_enable_tx_intr|uart_flush|uart_intr_config|uart_isr_register)\b")
RE_UART_SET_MODE = re.compile(r"\buart_set_mode\s*\(")
RE_GPIO_CFG = re.compile(r"CONFIG_DMT_BUS_(?:TX|DE)_GPIO")
RE_LINK_WRITE = re.compile(r"\bmb_link_write\w*")
RE_FC06_CODEC = re.compile(r"\bmb_rtu_(?:build_fc06|parse_fc06|fc06_)\w*")
RE_MB_FUNC = re.compile(r"\bMB_FUNC_\w*")
RE_RTU_FC_NAME = re.compile(r"\bMB_RTU_FC_\w*")
# #define of an MB_RTU_FC_* name; group 2 is the value when it is a lone hex literal (optional
# u/U suffix) followed only by blanks/comments up to the end of the line, else None.
# Accepts only '#' (not '%:'): a '%:define' is no accepted define, so its literal fails R8 (c).
_GAP = rf"(?:[ \t]|{_CMT})"
RE_FC_DEFINE = re.compile(
    rf"#{_GAP}*define{_GAP}+(MB_RTU_FC_\w+)"
    rf"(?:{_GAP}*(0[xX][0-9a-fA-F]+[uU]?)(?={_GAP}*(?:\n|\Z|//)))?")
# Hex literal incl. C++14 digit separators; any trailing identifier chars are suffix (u, l, UDL).
RE_HEX_LITERAL = re.compile(r"\b0[xX]([0-9a-fA-F](?:'?[0-9a-fA-F])*)\w*")
# Blanks and comments (a block comment between name and operator must not hide the assignment).
_FC_GAP = rf"(?:\s|{_CMT}|//[^\n]*\n)*"
_FC_NAME = r"(?:fc|func|func_code|function_code|command)"
_FC_FIELD = rf"(?<!\w){_FC_NAME}(?:{_FC_GAP}\))*{_FC_GAP}"
RE_FC_COMPOUND_ASSIGN = re.compile(rf"{_FC_FIELD}(<<|>>|[-+*/%&|^])=", re.IGNORECASE)
RE_FC_SIMPLE_ASSIGN = re.compile(rf"{_FC_FIELD}=(?!=)", re.IGNORECASE)
RE_FC_POSTFIX_STEP = re.compile(rf"{_FC_FIELD}(\+\+|--)", re.IGNORECASE)
# Prefix form: operator, optional '(', member path (a.b, p->q, a[i].c), then the name.
RE_FC_PREFIX_STEP = re.compile(
    rf"(\+\+|--){_FC_GAP}(?:\({_FC_GAP})*"
    rf"(?:\w+{_FC_GAP}(?:\[[^\]]*\]{_FC_GAP})*(?:\.|->){_FC_GAP})*(?<!\w){_FC_NAME}(?!\w)",
    re.IGNORECASE)
# GCC also accepts trailing blanks after the backslash (with a warning) and CRLF.
RE_CONTINUATION = re.compile(r"\\[ \t]*\r?\n")


def _line_of(text, offset):
    return text.count("\n", 0, offset) + 1


def _is_manifest(path):
    return os.path.basename(path) == MANIFEST_NAME


def _include_basename(include_target):
    return re.split(r"[/\\]", include_target.strip())[-1]


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
        yield m.start(), "R1", f"{m.group()} (esp-modbus master API) is banned"


def _check_r2(path, text):
    for m in RE_SLAVE.finditer(text):
        yield m.start(), "R2", f"{m.group()} (esp-modbus slave/TCP) is banned"
    for m in RE_INCLUDE.finditer(text):
        if _include_basename(m.group(1)) == "esp_modbus_slave.h":
            yield m.start(), "R2", "esp_modbus_slave.h include is banned"


def _is_modbus_header(basename):
    if basename == "esp_modbus_slave.h":
        return False  # reported by R2
    return basename.startswith("esp_modbus_") or basename == "mbcontroller.h"


def _check_r3(path, text):
    if _is_manifest(path):
        for m in RE_MANIFEST_MODBUS.finditer(text):
            yield m.start(), "R3", "esp-modbus in a component manifest is banned"
        return
    for m in RE_INCLUDE.finditer(text):
        if _is_modbus_header(_include_basename(m.group(1))):
            yield m.start(), "R3", f"{m.group(1)} (esp-modbus header) include is banned"


def _check_r4(path, text):
    in_port_src = path.startswith(RS485_PORT_SRC)
    for m in RE_UART_TX.finditer(text):
        if in_port_src and m.group() == UART_WRITE_ALLOWED:
            continue
        yield m.start(), "R4", f"{m.group()} is banned (UART TX/RTS/DTR/loopback/invert/low-level)"
    for m in RE_UART_REGISTER.finditer(text):
        yield m.start(), "R4", "raw UART register access is banned"
    for m in RE_UART_FIFO_REG_ACCESS.finditer(text):
        yield m.start(), "R4", "register macro on a UART FIFO register is banned"
    for m in RE_INCLUDE.finditer(text):
        if _include_basename(m.group(1)) in UART_LOW_LEVEL_HEADERS:
            yield m.start(), "R4", f"{m.group(1)} (low-level UART header) include is banned"


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
    if path in (WRITE_FILE, WRITE_HEADER, BUS_WRITE_FILE):
        return
    for m in RE_LINK_WRITE.finditer(text):
        yield m.start(), "R7", f"{m.group()} only allowed in {WRITE_FILE}, {WRITE_HEADER} and {BUS_WRITE_FILE}"


def _check_r8_names(path, text):
    """R8 (a) and (b): esp-modbus FC constants and the MB_RTU_FC_* names."""
    for m in RE_MB_FUNC.finditer(text):
        yield m.start(), "R8", f"{m.group()} (esp-modbus function code) is banned"
    allowed = FC_DEFINE_LITERALS_BY_FILE.get(path, {})
    for m in RE_RTU_FC_NAME.finditer(text):
        if not path.startswith(MB_RTU_SRC):
            yield m.start(), "R8", f"{m.group()} only allowed in {MB_RTU_SRC}"
        elif m.group() not in allowed:
            yield m.start(), "R8", f"{m.group()} is not an allowed function-code name in {path}"


def _check_r8_defines(path, text):
    """R8 (b): each define of an allowed name must carry its literal.

    Returns (violations, offsets of the accepted value literals); the offsets are the only
    0x03/0x83 literals R8 (c) lets through.
    """
    violations, accepted = [], set()
    allowed = FC_DEFINE_LITERALS_BY_FILE.get(path, {})
    for m in RE_FC_DEFINE.finditer(text):
        name, value = m.group(1), m.group(2)
        if name not in allowed:
            continue  # reported by the name check
        expected = allowed[name]
        if value is None or value.lower().rstrip("u") != expected:
            violations.append((m.start(), "R8", f"{name} must be defined as literal {expected}"))
        else:
            accepted.add(m.start(2))
    return violations, accepted


def _check_r9(path, text):
    if path in (MB_RTU_WRITE_C, MB_RTU_WRITE_HEADER, WRITE_FILE):
        return
    for m in RE_FC06_CODEC.finditer(text):
        yield m.start(), "R9", f"{m.group()} only allowed in {MB_RTU_WRITE_C}, {MB_RTU_WRITE_HEADER} and {WRITE_FILE}"


def _check_r8_literals(text, accepted_offsets):
    """R8 (c): hex literals with a function-code value."""
    for m in RE_HEX_LITERAL.finditer(text):
        if m.start() in accepted_offsets:
            continue
        value = int(m.group(1).replace("'", ""), 16)
        if value in READ_FC_VALUES or value in WRITE_FC_VALUES:
            yield m.start(), "R8", f"function-code literal {m.group()} only allowed as the MB_RTU_FC_READ_HOLDING[_EXC] define in {MB_RTU_SRC}"


def _check_r8_assignments(path, text):
    """R8 (d): ++/--, compound (and, outside mb_rtu, simple) assignment to a function-code field."""
    for m in RE_FC_COMPOUND_ASSIGN.finditer(text):
        yield m.start(), "R8", f"compound assignment '{m.group(1)}=' to a function-code field is banned"
    for regex in (RE_FC_POSTFIX_STEP, RE_FC_PREFIX_STEP):
        for m in regex.finditer(text):
            yield m.start(), "R8", f"'{m.group(1)}' on a function-code field is banned"
    if not path.startswith(FC_SIMPLE_ASSIGN_COMPONENTS):
        return
    for m in RE_FC_SIMPLE_ASSIGN.finditer(text):
        yield m.start(), "R8", "assignment to a function-code field is banned here"


def _check_r8(path, text):
    yield from _check_r8_names(path, text)
    define_violations, accepted = _check_r8_defines(path, text)
    yield from define_violations
    if not path.startswith(FC_COMPONENTS):
        return
    yield from _check_r8_literals(text, accepted)
    yield from _check_r8_assignments(path, text)


CHECKS = (_check_r1, _check_r2, _check_r3, _check_r4,
          _check_r5, _check_r6, _check_r7, _check_r8, _check_r9)
MANIFEST_CHECKS = (_check_r3,)


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
    if _is_manifest(path):
        spliced, removed_at, checks = text, [], MANIFEST_CHECKS
    else:
        spliced, removed_at = _splice_continuations(text)
        checks = CHECKS
    found = set()
    for check in checks:
        for offset, rule, message in check(path, spliced):
            hidden = sum(1 for pos in removed_at if pos <= offset)
            found.add((_line_of(spliced, offset) + hidden, rule, message))
    return sorted(found)


def _is_scanned_name(name):
    return name.endswith(SCAN_SUFFIXES) or name == MANIFEST_NAME


def _iter_sources(root):
    for top in SCAN_ROOTS:
        for dirpath, dirnames, filenames in os.walk(os.path.join(root, top)):
            dirnames[:] = sorted(d for d in dirnames
                                 if d not in SKIP_DIRS and not RE_BUILD_DIR.fullmatch(d))
            for name in sorted(filenames):
                if _is_scanned_name(name):
                    yield os.path.join(dirpath, name)


def scan_tree(root):
    violations = []
    for full in _iter_sources(root):
        rel = os.path.relpath(full, root).replace(os.sep, "/")
        with open(full, encoding="utf-8", errors="replace") as f:
            text = f.read()
        violations += [(rel, line, rule, msg) for line, rule, msg in check_source(rel, text)]
    return violations


MB_LINK_C = "components/mb_link/src/mb_link.c"
RS485_C = "components/rs485_port/src/rs485_port.c"
RD_C = "components/daikin_regs/src/daikin_regs.c"
MB_LINK_YML = "components/mb_link/idf_component.yml"
MAIN_C = "main/main.c"
COMPOUND_OPERATORS = ("|=", "&=", "^=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=")
WRITE_FC_SNIPPETS = ("0x05", "0x06", "0x0F", "0x10", "0x15", "0x16", "0x17")
FC_STEP_COMPONENT_FILES = (MB_RTU_C, MB_LINK_C, RS485_C)
FC_STEP_BAD_SNIPPETS = ("fc++;", "fc--;", "++fc;", "--fc;", "r.command ++;", "++ r.command;",
                        "--p->func_code;", "(fc)++;", "++(fc);", "fc /* c */ ++;",
                        "++ /* c */ fc;", "++a[i].fc;", "FUNC++;")
NEW_R5_CALLS = ("uart_set_hw_flow_ctrl(p, UART_HW_FLOWCTRL_DISABLE, 0);", "uart_set_baudrate(p, 9600);",
                "uart_set_parity(p, UART_PARITY_DISABLE);", "uart_set_word_length(p, UART_DATA_8_BITS);",
                "uart_set_stop_bits(p, UART_STOP_BITS_1);", "uart_set_tx_idle_num(p, 0);",
                "uart_enable_tx_intr(p, 1, 0);", "uart_flush(p);", "uart_intr_config(p, &c);",
                "uart_isr_register(p, f, a, 0, &h);")
# Task 014 plans these calls in rs485_port/src: the new rules must let them through.
PORT_OPEN_AND_TRANSACT = (
    "uart_config_t cfg = { .baud_rate = 9600, .data_bits = UART_DATA_8_BITS, "
    ".parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1, "
    ".flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT }; "
    "gpio_set_level(de, 0); gpio_set_direction(de, GPIO_MODE_OUTPUT); "
    "uart_param_config(p, &cfg); uart_driver_install(p, 256, 0, 0, NULL, 0); "
    "uart_set_mode(p, UART_MODE_RS485_HALF_DUPLEX); uart_set_pin(p, tx, rx, de, UART_PIN_NO_CHANGE); "
    "uart_set_rx_full_threshold(p, 1); gpio_pullup_en(rx); uart_get_buffered_data_len(p, &n); "
    "uart_flush_input(p); n = uart_write_bytes(p, tx, 8); uart_wait_tx_done(p, t); "
    "uart_read_bytes(p, b, 1, t); int64_t now = esp_timer_get_time(); uart_driver_delete(p);")
PORT_SILENCE_CHECK = (
    "uart_driver_install(p, 256, 0, 8, &q, 0); uart_set_mode(p, UART_MODE_UART); "
    "uart_set_pin(p, UART_PIN_NO_CHANGE, rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE); "
    "if (xQueueReceive(q, &ev, t) && (ev.type == UART_DATA || ev.type == UART_FRAME_ERR "
    "|| ev.type == UART_PARITY_ERR || ev.type == UART_BREAK || ev.type == UART_FIFO_OVF "
    "|| ev.type == UART_BUFFER_FULL)) { gap_bytes += ev.size; faults++; } uart_driver_delete(p);")
# Repetition-heavy inputs for the timing guard (each case must finish within CASE_TIME_LIMIT_S).
REPEAT = 5000
CASE_TIME_LIMIT_S = 5.0

# (virtual path, snippet, expected set of rules); empty set = must pass.
SELF_TEST_CASES = (
    # R1: no esp-modbus master API anywhere
    (MB_LINK_C, "mbc_master_send_request(h, &r, d);", {"R1"}),
    (MB_LINK_C, "mbc_master_set_descriptor(h, d, 1);", {"R1"}),
    (MB_LINK_C, "mbc_master_create_serial(&c, &h);", {"R1"}),
    (MB_LINK_C, "mbc_master_start(h); mbc_master_stop(h); mbc_master_delete(h);", {"R1"}),
    (MB_LINK_C, "mbc_master_set_parameter(h, 1, 2, 3);", {"R1"}),
    ("components/mb_link/include/mb_link.h", "mbc_master_start(h);", {"R1"}),
    (MAIN_C, "mbc_master_start(h);", {"R1"}),
    (MAIN_C, "mbm_rq_read_holding_reg(i, 1, 0, 1, 0);", {"R1"}),
    (MAIN_C, "mbc_dummy();", {"R1"}),
    (MAIN_C, "mb_link_read(1, 0, 1, v, &e);", set()),
    (MAIN_C, "mb_rtu_build_fc03(1, 0, 1, b);", set()),
    # R2
    (MB_LINK_C, "mbc_slave_start(h);", {"R2"}),
    (MAIN_C, "mbs_init();", {"R2"}),
    (MAIN_C, "mbc_tcp_master_x();", {"R2"}),
    (MB_LINK_C, '#include "esp_modbus_slave.h"', {"R2"}),
    (MB_LINK_C, '#include "some/dir/esp_modbus_slave.h"', {"R2"}),
    # R3: no esp-modbus header anywhere, no esp-modbus in manifests
    (MB_LINK_C, '#include "esp_modbus_master.h"', {"R3"}),
    (MB_LINK_C, "#include <esp_modbus_common.h>", {"R3"}),
    (MB_LINK_C, '#include "mbcontroller.h"', {"R3"}),
    (MB_LINK_C, '#  include "esp_modbus_master.h"', {"R3"}),
    (MB_LINK_C, '#include_next <esp_modbus_master.h>', {"R3"}),
    (MB_LINK_C, '#include "esp_modbus/esp_modbus_master.h"', {"R3"}),
    (MB_LINK_C, '#include "esp_modbus_future.h"', {"R3"}),
    ("components/mb_link/include/mb_link.h", '#include "esp_modbus_master.h"', {"R3"}),
    (MAIN_C, '#include "esp_modbus_common.h"', {"R3"}),
    (MAIN_C, '#include "mb_link.h"', set()),
    ("components/mb_link/idf_component.yml", '  espressif/esp-modbus:\n    version: "==2.1.4"', {"R3"}),
    ("components/mb_link/idf_component.yml", '# uses esp-modbus', {"R3"}),
    ("main/idf_component.yml", "  espressif/esp-modbus: '*'", {"R3"}),
    ("components/mb_link/idf_component.yml", 'dependencies:\n  idf: ">=6.0"', set()),
    # R4: only the exact uart_write_bytes in rs485_port/src
    (RS485_C, "uart_write_bytes(p, b, 8);", set()),
    ("components/rs485_port/include/rs485_port.h", "uart_write_bytes(p, b, 8);", {"R4"}),
    (MB_LINK_C, "uart_write_bytes(p, b, 8);", {"R4"}),
    (MB_RTU_C, "uart_write_bytes(p, b, 8);", {"R4"}),
    (RS485_C, "uart_write_bytes_with_break(p, b, 8, 2);", {"R4"}),
    (RS485_C, "uart_tx_chars(p, b, 1);", {"R4"}),
    (RS485_C, "uart_\\\nwrite_bytes(p, b, 8); uart_write_bytes_with_break(p, b, 8, 2);", {"R4"}),
    ("components/rs485_port/src/sub/x.c", "uart_write_bytes(p, b, 8);", set()),
    (MAIN_C, 'uart_write_bytes(1, "x", 1);', {"R4"}),
    (MB_LINK_C, "uart_set_rts(1, 0);", {"R4"}),
    (MAIN_C, "uart_set_dtr(1, 0);", {"R4"}),
    (MAIN_C, "uart_set_loop_back(1, true);", {"R4"}),
    (MAIN_C, "uart_set_line_inverse(1, 0);", {"R4"}),
    (RS485_C, "_uart_set_pin6(1);", {"R4"}),
    (MAIN_C, "uart_write_bytes_with_break(1, b, 1, 2);", {"R4"}),
    # R5
    (RS485_C, "uart_set_mode(p, UART_MODE_RS485_HALF_DUPLEX);", set()),
    (RS485_C, "uart_set_mode(p,\n    UART_MODE_UART);", set()),
    (RS485_C, "uart_driver_install(p, 256, 0, 0, NULL, 0); uart_read_bytes(p, b, 1, 1);", set()),
    (RS485_C, "uart_set_mode(p, UART_MODE_RS485_APP_CTRL);", {"R5"}),
    (RS485_C, "uart_set_mode(p, mode_var);", {"R5"}),
    (MB_LINK_C, "uart_set_mode(p, UART_MODE_UART);", {"R5"}),
    (MAIN_C, "uart_driver_install(1, 256, 0, 0, NULL, 0);", {"R5"}),
    (MAIN_C, "uart_set_pin(1, 5, 4, 6, -1);", {"R5"}),
    (MB_LINK_C, "uart_param_config(1, &cfg);", {"R5"}),
    (MAIN_C, "uart_read_bytes(1, b, 1, 1);", {"R5"}),
    (MAIN_C, "uart_driver_delete(1);", {"R5"}),
    # R6
    (MAIN_C, "int g = CONFIG_DMT_BUS_TX_GPIO;", set()),
    (RS485_C, "int g = CONFIG_DMT_BUS_DE_GPIO;", set()),
    (MB_LINK_C, "int g = CONFIG_DMT_BUS_DE_GPIO;", {"R6"}),
    ("components/bus_task/src/x.c", "int g = CONFIG_DMT_BUS_TX_GPIO;", {"R6"}),
    ("components/bus_task/src/x.c", "int g = CONFIG_DMT_BUS_RX_GPIO;", set()),
    # R7
    ("components/mb_link/src/mb_link_write.c", "int mb_link_write(int a);", set()),
    ("components/mb_link/include/mb_link_write.h", "int mb_link_write(int a);", set()),
    (MB_LINK_C, "mb_link_write(1, 2, 3);", {"R7"}),
    (MAIN_C, "mb_link_write_reg(1);", {"R7"}),
    (BUS_WRITE_FILE, "mb_link_write(1, 231, 215, &exc);", set()),
    ("components/bus_task/src/bus_write.h", "mb_link_write(1, 231, 215, &exc);", {"R7"}),
    ("components/bus_task/src/mb_console.c", "mb_link_write(1, 231, 215, &exc);", {"R7"}),
    ("components/bus_task/src/bus_task.c", "mb_link_write_x();", {"R7"}),
    # R9: FC06 codec names only in mb_rtu_write.c/.h and mb_link_write.c
    (MB_RTU_WRITE_C, "int n = mb_rtu_build_fc06(a, r, v, o); mb_rtu_parse_fc06_reply(q, x, l, &e); mb_rtu_fc06_reply_len(x, n);", set()),
    (MB_RTU_WRITE_HEADER, "int mb_rtu_build_fc06(uint8_t a, uint16_t r, uint16_t v, uint8_t o[8]);", set()),
    (WRITE_FILE, "mb_rtu_build_fc06(a, r, v, o); mb_rtu_parse_fc06_reply(q, x, l, &e); mb_rtu_fc06_reply_len(x, n);", set()),
    ("components/bus_task/src/bus_task.c", "mb_rtu_build_fc06(a, r, v, o);", {"R9"}),
    ("components/bus_task/src/bus_write.c", "mb_rtu_parse_fc06_reply(q, x, l, &e);", {"R9"}),
    (WRITE_HEADER, "mb_rtu_fc06_reply_len(x, n);", {"R9"}),
    (MB_LINK_C, "mb_rtu_build_fc06(a, r, v, o);", {"R9"}),
    (MAIN_C, "mb_rtu_fc06_x();", {"R9"}),
    (MAIN_C, "mb_rtu_build_fc03(1, 0, 1, b); mb_rtu_parse_fc03_reply(1, 1, r, n, v, &e);", set()),
    # R8 (b) for the write codec: its own two defines only, in mb_rtu_write.c only
    (MB_RTU_WRITE_C, "#define MB_RTU_FC_WRITE_SINGLE 0x06u\n#define MB_RTU_FC_WRITE_SINGLE_EXC 0x86u", set()),
    (MB_RTU_WRITE_C, "#define MB_RTU_FC_WRITE_SINGLE 0x06u\nf[1] = MB_RTU_FC_WRITE_SINGLE; if (r[1] == MB_RTU_FC_WRITE_SINGLE_EXC) {}", set()),
    (MB_RTU_WRITE_C, "#define MB_RTU_FC_WRITE_SINGLE 0x07u", {"R8"}),
    (MB_RTU_WRITE_C, "#define MB_RTU_FC_WRITE_SINGLE_EXC 0x06u", {"R8"}),
    (MB_RTU_WRITE_C, "#define MB_RTU_FC_WRITE_SINGLE 0x06u\nx = 0x06u;", {"R8"}),
    (MB_RTU_WRITE_C, "#define MB_RTU_FC_WRITE_SINGLE_EXC 0x86u\nx = 0x86;", {"R8"}),
    (MB_RTU_WRITE_C, "#define MB_RTU_FC_READ_HOLDING 0x03u", {"R8"}),
    (MB_RTU_WRITE_C, "x = 0x10;", {"R8"}),
    (MB_RTU_WRITE_C, "#define MB_RTU_FC_WRITE_SINGLE 0x06u\n#define MB_RTU_FC_WRITE_MULTI 0x10u", {"R8"}),
    (MB_RTU_C, "#define MB_RTU_FC_WRITE_SINGLE 0x06u", {"R8"}),
    (MB_RTU_C, "x = MB_RTU_FC_WRITE_SINGLE_EXC;", {"R8"}),
    ("components/mb_rtu/include/mb_rtu_write.h", "#define MB_RTU_FC_WRITE_SINGLE 0x06u", {"R8"}),
    (MB_LINK_C, "x = MB_RTU_FC_WRITE_SINGLE;", {"R8"}),
    (WRITE_FILE, "b = 0x06;", {"R8"}),
    (WRITE_FILE, "b = 0x86;", {"R8"}),
    (WRITE_FILE, "b = 0x10;", {"R8"}),
    (MB_LINK_C, "b = 0x10;", {"R8"}),
    (RS485_C, "b = 0x86;", {"R8"}),
    (MB_RTU_C, "b = 0x10;", {"R8"}),
    # R8 (a): esp-modbus constants anywhere
    (MB_LINK_C, "x = MB_FUNC_READ_HOLDING_REGISTER;", {"R8"}),
    (MAIN_C, "x = MB_FUNC_WRITE_SINGLE_REGISTER;", {"R8"}),
    (MB_LINK_C, "x = MB_FUNC_WRITE_SINGLE_REGISTER;", {"R8"}),
    (RS485_C, "x = MB_FUNC_WRITE_MULTIPLE_REGISTERS;", {"R8"}),
    ("components/mb_link/src/mb_link_write.c", "r.command = 0x06; x = MB_FUNC_WRITE_SINGLE_REGISTER;", {"R8"}),
    # R8 (b): MB_RTU_FC_* names and their defines
    (MB_RTU_C, "#define MB_RTU_FC_READ_HOLDING 0x03u\n#define MB_RTU_FC_READ_HOLDING_EXC 0x83u", set()),
    (MB_RTU_C, "#define MB_RTU_FC_READ_HOLDING 0x03", set()),
    (MB_RTU_C, "#define MB_RTU_FC_READ_HOLDING 0X03U /* read */", set()),
    (MB_RTU_C, "#define MB_RTU_FC_READ_HOLDING_EXC 0x83 // exception", set()),
    (MB_RTU_C, "#define MB_RTU_FC_READ_HOLDING 0x03u\nf[1] = MB_RTU_FC_READ_HOLDING;", set()),
    (MB_RTU_C, "if (rx[1] == MB_RTU_FC_READ_HOLDING_EXC) {}", set()),
    (MB_RTU_C, "#define MB_RTU_FC_READ_HOLDING 0x06u", {"R8"}),
    (MB_RTU_C, "#define MB_RTU_FC_READ_HOLDING_EXC 0x03u", {"R8"}),
    (MB_RTU_C, "#define MB_RTU_FC_READ_HOLDING 0x83u", {"R8"}),
    (MB_RTU_C, "#define MB_RTU_FC_READ_HOLDING 3", {"R8"}),
    (MB_RTU_C, "#define MB_RTU_FC_READ_HOLDING (0x03u)", {"R8"}),
    (MB_RTU_C, "#define MB_RTU_FC_READ_HOLDING 0x03u + 0", {"R8"}),
    (MB_RTU_C, "#define MB_RTU_FC_READ_HOLDING", {"R8"}),
    (MB_RTU_C, "#define /* x */ MB_RTU_FC_READ_HOLDING 6", {"R8"}),
    (MB_RTU_C, "#define MB_RTU_FC_READ_HOLDING 0x003u", {"R8"}),
    (MB_RTU_C, "#define MB_RTU_FC_WRITE_SINGLE 0x06u", {"R8"}),
    (MB_RTU_C, "x = MB_RTU_FC_OTHER;", {"R8"}),
    ("components/mb_rtu/include/mb_rtu.h", "#define MB_RTU_FC_READ_HOLDING 0x03u", {"R8"}),
    (MB_LINK_C, "out[1] = MB_RTU_FC_READ_HOLDING;", {"R8"}),
    (MAIN_C, "x = MB_RTU_FC_READ_HOLDING;", {"R8"}),
    # R8 (c): function-code hex literals in mb_rtu / mb_link / rs485_port
    (MB_LINK_C, "b = 0x06;", {"R8"}),
    (MB_LINK_C, "b = 0X0f;", {"R8"}),
    (MB_LINK_C, "b = 0x0010u;", {"R8"}),
    (MB_LINK_C, "b = 0x03;", {"R8"}),
    (MB_LINK_C, "b = 0x83;", {"R8"}),
    (MB_LINK_C, "b = 0x0'6;", {"R8"}),
    (MB_LINK_C, "b = 0x06UL;", {"R8"}),
    (MB_LINK_C, "b = 0x06_fc;", {"R8"}),
    (MB_LINK_C, "b = (0x16);", {"R8"}),
    (MB_LINK_C, "/* FC 0x06 */", {"R8"}),
    (RS485_C, "n = 0x17;", {"R8"}),
    (MB_LINK_C, "x = 0x0300;", set()),
    (MB_LINK_C, "x = 0x04; y = 0x07; z = 0x1F; w = 0x84; v = 0x0A;", set()),
    (MB_LINK_C, "x = a0x06;", set()),
    ("components/daikin_regs/src/daikin_regs.c", "x = 0x06;", set()),
    (MAIN_C, "x = 0x06;", set()),
    (MB_RTU_C, "crc ^= 0xA001u; c = 0xFFFFu; m = 0x80u;", set()),
    (MB_RTU_C, "if (rx[1] == 0x03) {}", {"R8"}),
    (MB_RTU_C, "if (rx[1] == 0x83) {}", {"R8"}),
    (MB_RTU_C, "#define MB_RTU_FC_READ_HOLDING 0x03u\nif (rx[1] == 0x03u) {}", {"R8"}),
    (MB_RTU_C, "#define MB_RTU_FC_READ_HOLDING 0x03u\n#define MB_RTU_FC_READ_HOLDING_EXC 0x83u\nx = 0x83;", {"R8"}),
    ("components/mb_rtu/include/mb_rtu.h", "enum { K = 0x03 };", {"R8"}),
    # R8 (d): assignments to function-code fields
    (MB_RTU_C, "uint8_t fc = rx[1];", set()),
    (MB_RTU_C, "r.command = rx[1];", set()),
    (MB_LINK_C, "fc = 3;", {"R8"}),
    (MB_LINK_C, "r.command = 0x03;", {"R8"}),
    (MB_LINK_C, "uint8_t fc = rx[1];", {"R8"}),
    (MB_LINK_C, "{ .command = x, .y = 1 }", {"R8"}),
    (MB_LINK_C, "command = other;", {"R8"}),
    (MB_LINK_C, "r.command =\n    other;", {"R8"}),
    (MB_LINK_C, "p->func = y;", {"R8"}),
    (MB_LINK_C, "p->func_code = y;", {"R8"}),
    (MB_LINK_C, "Function_Code = y;", {"R8"}),
    (MB_LINK_C, "FC=y;", {"R8"}),
    (MB_LINK_C, "(fc) = y;", {"R8"}),
    (MB_LINK_C, "fc /* c */ = y;", {"R8"}),
    (MB_LINK_C, "fc // c\n = y;", {"R8"}),
    (MB_LINK_C, "r.command /* c */ |= y;", {"R8"}),
    (MB_LINK_C, "(fc /* c */) = y;", {"R8"}),
    (MB_LINK_C, "if (fc /* c */ == y) {}", set()),
    (RS485_C, "fc = y;", {"R8"}),
    (RS485_C, "r.func = y;", {"R8"}),
    (MB_LINK_C, "if (fc == x || fc >= y || fc != z || fc <= w) {}", set()),
    (MB_LINK_C, "if (r.command == a || r.command >= b || r.command != c || r.command <= d) {}", set()),
    (MB_LINK_C, "if (g(fc) == 2) {} else if (h(fc)) y = 1;", set()),
    (MB_LINK_C, "int fcx = 1; int xfc = 2; int fc_len = 3; int func_codes = 4; MB_RTU_FC = 5;", set()),
    (MB_LINK_C, "int commands = 1; int my_command = 2;", set()),
    ("components/daikin_regs/src/daikin_regs.c", "fc = 3; command |= 1;", set()),
    (MAIN_C, "fc = 3; command |= 1;", set()),
    ("components/mb_link/include/mb_link.h", "int fc;", set()),
    # comments, strings, continuations, multi-line calls
    (MAIN_C, "// uart_write_bytes(1, b, 1);", {"R4"}),
    (MAIN_C, 'puts("uart_write_bytes");', {"R4"}),
    (MAIN_C, "uart_\\\nwrite_bytes(1, b, 1);", {"R4"}),
    (RS485_C, "uart_set_mode(p,\n    UART_MODE_RS485_APP_CTRL);", {"R5"}),
    (MAIN_C, "uart_\\\r\nwrite_bytes(1);", {"R4"}),
    (MAIN_C, "uart_\\ \t\nwrite_bytes(1);", {"R4"}),
    (MB_LINK_C, "r.com\\\nmand = y;", {"R8"}),
    (MB_LINK_C, "b = 0x0\\\n6;", {"R8"}),
    # A1: include directive detection (comments, digraph, import, include_next)
    (MB_LINK_C, '#include /*c*/ "esp_modbus_master.h"', {"R3"}),
    (MB_LINK_C, "#/**/include <esp_modbus_master.h>", {"R3"}),
    (MB_LINK_C, "# /* a */ include /* b */ <mbcontroller.h>", {"R3"}),
    (MB_LINK_C, '#/* multi\nline */include "esp_modbus_master.h"', {"R3"}),
    (MB_LINK_C, '%:include "esp_modbus_master.h"', {"R3"}),
    (MB_LINK_C, "%: include <mbcontroller.h>", {"R3"}),
    (MB_LINK_C, '#import "esp_modbus_master.h"', {"R3"}),
    (MB_LINK_C, "#include_next /**/ <esp_modbus_common.h>", {"R3"}),
    (MB_LINK_C, "#import <esp_modbus_slave.h>", {"R2"}),
    (MB_LINK_C, '%:include "esp_modbus_slave.h"', {"R2"}),
    (MAIN_C, '#include /*c*/ "mb_link.h"', set()),
    (MB_RTU_C, "%:define MB_RTU_FC_READ_HOLDING 0x03u", {"R8"}),
    # A2: timing guard, one name/directive per case with thousands of repeated comments
    (MB_LINK_C, "fc" + " /* a */" * REPEAT + " ;", set()),
    (MB_LINK_C, "fc" + " /* a */" * REPEAT + " = y;", {"R8"}),
    (MB_RTU_C, "r.command" + " /* a */" * REPEAT + " |= 1;", {"R8"}),
    (MB_LINK_C, "#" + "/**/" * REPEAT + "include <esp_modbus_master.h>", {"R3"}),
    (MB_LINK_C, "#" + "/**/" * REPEAT + "x", set()),
    (MB_RTU_C, "#define" + " /* x */" * REPEAT + " MB_RTU_FC_READ_HOLDING 0x03u", set()),
    (MB_RTU_C, "fc" + " /* a */" * REPEAT + " ++;", {"R8"}),
    (MB_LINK_C, "fc" + ")" * REPEAT + " ;", set()),
    (MB_LINK_C, "++" + " /* a */" * REPEAT + " fc;", {"R8"}),
    (MB_LINK_C, "++" + "(" * REPEAT + "fc;", {"R8"}),
    (MB_LINK_C, "++" + "a." * REPEAT + "x;", set()),
    (MB_LINK_C, "++" + "a[0]." * REPEAT + "fc;", {"R8"}),
    (MB_LINK_C, "++a[b[1]].fc;", set()),  # documented limit: nested brackets are not followed
    (MB_LINK_C, "UART1" + " /* a */" * REPEAT + " .x = 0;", {"R4"}),
    # R4 follow-up: ROM UART routes, pointer register access, FIFO register macros, uart_reg.h
    (RS485_C, "esp_rom_output_tx_one_char('x');", {"R4"}),
    (MAIN_C, "esp_rom_output_tx_one_char('x');", {"R4"}),
    (MAIN_C, "esp_rom_uart_putc('x');", {"R4"}),
    (RS485_C, "esp_rom_uart_tx_one_char('x');", {"R4"}),
    (RS485_C, "esp_rom_install_uart_printf();", {"R4"}),
    (MAIN_C, "esp_rom_install_channel_putc(1, f);", {"R4"}),
    (RS485_C, "UART1->fifo.val = 0;", {"R4"}),
    (MAIN_C, "UART0 -> conf0.val = 0;", {"R4"}),
    (MAIN_C, "UART2 /* c */ ->conf0.val = 0;", {"R4"}),
    (RS485_C, "WRITE_PERI_REG(UART_FIFO_REG(1), 0x41);", {"R4"}),
    (MAIN_C, "REG_WRITE(UART_FIFO_AHB_REG(1), 0x41);", {"R4"}),
    (MAIN_C, "REG_WRITE(\n    UART_FIFO_AHB_REG(1),\n    0x41);", {"R4"}),
    (RS485_C, "REG_READ(UART_FIFO_REG(1));", {"R4"}),
    (MAIN_C, "x = READ_PERI_REG(UART_FIFO_AHB_REG(1));", {"R4"}),
    (MAIN_C, "REG_WRITE(" + "a" * 150 + ", UART_FIFO_REG(1));", {"R4"}),
    (RS485_C, '#include "soc/uart_reg.h"', {"R4"}),
    (MAIN_C, "#include <soc/uart_reg.h>", {"R4"}),
    (MAIN_C, "esp_rom_delay_us(10);", set()),
    (RS485_C, "esp_rom_delay_us(10);", set()),
    (MAIN_C, "int n = UART_NUM_1;", set()),
    (MAIN_C, "x = UART_FIFO_REG_SIZE;", set()),
    (MAIN_C, "REG_WRITE(GPIO_OUT_REG, 1); x = REG_READ(GPIO_IN_REG);", set()),
    (MAIN_C, "REG_WRITE(GPIO_OUT_REG, 1);\nx = UART_FIFO_REG(1);", set()),
    (MAIN_C, "REG_WRITE(" + "a" * 300 + ", UART_FIFO_REG(1));", set()),
    (MAIN_C, "REG_WRITE(" * REPEAT + "x;", set()),
    (MAIN_C, "REG_WRITE(" * REPEAT + "UART_FIFO_REG(1);", {"R4"}),
    (MAIN_C, "READ_PERI_REG(" * REPEAT + "x;", set()),
    (MAIN_C, "UART1" + " /* a */" * REPEAT + " ->x = 0;", {"R4"}),
    # A3: ++/-- that must pass
    (MB_RTU_C, "i++; n--; ++i; fc_len++; ++fcx; --xfc; ++commands;", set()),
    (MB_RTU_C, "x = fc + +y; z = fc - -1;", set()),
    (MB_RTU_C, "if (fc == 3) {}", set()),
    (MB_RTU_C, "a = b + ++i; c = d - --j;", set()),
    (RD_C, "fc++; --command;", set()),
    (MAIN_C, "fc++; --command;", set()),
    # A4: manifest spellings
    (MB_LINK_YML, '  espressif/esp_modbus: "*"', {"R3"}),
    (MB_LINK_YML, '  ESPRESSIF/ESP-MODBUS: "*"', {"R3"}),
    (MB_LINK_YML, "# Esp_Modbus", {"R3"}),
    (MB_LINK_YML, '  espressif/modbus_other: "*"', set()),
    # B / R4: low-level UART access banned everywhere, including rs485_port/src
    (RS485_C, "uart_ll_write_txfifo(h, b, 8);", {"R4"}),
    (MB_LINK_C, "uart_ll_write_txfifo(h, b, 8);", {"R4"}),
    (MAIN_C, "uart_ll_write_txfifo(h, b, 8);", {"R4"}),
    (RS485_C, "uart_hal_write_txfifo(&h, b, 8, &n);", {"R4"}),
    (RS485_C, "esp_rom_uart_tx_one_char('x');", {"R4"}),
    (MAIN_C, "esp_rom_uart_tx_one_char('x');", {"R4"}),
    (MAIN_C, "uart_tx_one_char('x');", {"R4"}),
    (RS485_C, "UART1.fifo.val = 0;", {"R4"}),
    (MAIN_C, "UART0 . conf0.val = 0;", {"R4"}),
    (RS485_C, '#include "soc/uart_struct.h"', {"R4"}),
    (RS485_C, "#include <hal/uart_ll.h>", {"R4"}),
    (MB_LINK_C, '#include "hal/uart_hal.h"', {"R4"}),
    (MAIN_C, '#include /*c*/ "hal/uart_ll.h"', {"R4"}),
    (RS485_C, '#include "uart_ll.h"', {"R4"}),
    (MAIN_C, "int p = UART_NUM_1; int q = UART_PIN_NO_CHANGE; int r = UART10;", set()),
    (RS485_C, '#include "driver/uart.h"', set()),
    (RS485_C, '#include "hal/uart_types.h"', set()),
    # B / R5: new setup names allowed only in rs485_port; uart_flush_input stays allowed
    (RS485_C, "uart_flush_input(p);", set()),
    (MAIN_C, "uart_flush_input(p);", set()),
    # 014 compatibility: the planned rs485_port/src calls pass
    (RS485_C, PORT_OPEN_AND_TRANSACT, set()),
    (RS485_C, PORT_SILENCE_CHECK, set()),
) + tuple(
    (path, snippet, {"R8"})
    for path in FC_STEP_COMPONENT_FILES for snippet in FC_STEP_BAD_SNIPPETS
) + tuple(
    (path, call, {"R5"}) for path in (MAIN_C, MB_LINK_C) for call in NEW_R5_CALLS
) + tuple(
    (RS485_C, call, set()) for call in NEW_R5_CALLS
) + tuple(
    (path, f"r.command {op} 1;", {"R8"})
    for path in (MB_RTU_C, MB_LINK_C, RS485_C) for op in COMPOUND_OPERATORS
) + tuple(
    (MB_RTU_C, f"fc {op} 1;", {"R8"}) for op in COMPOUND_OPERATORS
) + tuple(
    (MB_LINK_C, f"b = {code};", {"R8"}) for code in WRITE_FC_SNIPPETS
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
        ("components/c/idf_component.yml", True),
        ("main/idf_component.yml", True),
        ("components/c/other.yml", False),
        ("components/c/managed_components/idf_component.yml", False),
        ("components/c/build/idf_component.yml", False),
    )


def _walker_marker(rel):
    """File content that is flagged (R3 in manifests, else R4) if the file is scanned."""
    return "espressif/esp-modbus\n" if rel.endswith(".yml") else "uart_write_bytes(1, b, 1);\n"


def _tree_walker_test():
    failures = 0
    with tempfile.TemporaryDirectory() as root:
        cases = _tree_test_expectations()
        for rel, _ in cases:
            full = os.path.join(root, rel)
            os.makedirs(os.path.dirname(full), exist_ok=True)
            with open(full, "w", encoding="utf-8") as f:
                f.write(_walker_marker(rel))
        flagged = {rel for rel, _, _, _ in scan_tree(root)}
        for rel, scanned in cases:
            if (rel in flagged) != scanned:
                failures += 1
                print(f"self-test FAIL tree walker {rel}: scanned={rel in flagged}, expected {scanned}")
    return failures, len(cases)


def self_test():
    failures = 0
    for path, snippet, expected in SELF_TEST_CASES:
        started = time.monotonic()
        got = {rule for _, rule, _ in check_source(path, snippet)}
        elapsed = time.monotonic() - started
        if got != expected:
            failures += 1
            print(f"self-test FAIL {path}: expected {sorted(expected)}, got {sorted(got)}: {snippet[:80]!r}")
        if elapsed > CASE_TIME_LIMIT_S:
            failures += 1
            print(f"self-test FAIL {path}: took {elapsed:.2f} s (limit {CASE_TIME_LIMIT_S} s): {snippet[:80]!r}")
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

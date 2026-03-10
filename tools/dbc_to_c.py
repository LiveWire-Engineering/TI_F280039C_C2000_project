#!/usr/bin/env python3
"""
DBC to C Code Generator for TI F280039C CAN Driver

Parses a Vector DBC file and generates C source/header files containing:
  - Message ID definitions
  - Signal struct typedefs with pack/unpack helpers
  - Transmit and receive mailbox configuration tables

Usage:
    python dbc_to_c.py <input.dbc> [--outdir <dir>] [--node <ECU_name>]

The generated files (can_message.h / can_message.c) are designed to be
compiled alongside the CAN driver module (can_driver.c/h) for the
TI F280039C C2000 microcontroller.
"""

import re
import sys
import os
import argparse

# ---------------------------------------------------------------------------
# DBC model classes
# ---------------------------------------------------------------------------

class Signal:
    """Represents one SG_ entry inside a message."""
    def __init__(self, name, start_bit, length, byte_order, is_signed,
                 factor, offset, minimum, maximum, unit, receivers):
        self.name = name
        self.start_bit = int(start_bit)
        self.length = int(length)
        self.byte_order = byte_order   # '1' = little-endian, '0' = big-endian
        self.is_signed = is_signed     # '+' = unsigned, '-' = signed
        self.factor = float(factor)
        self.offset = float(offset)
        self.minimum = float(minimum)
        self.maximum = float(maximum)
        self.unit = unit
        self.receivers = receivers

    @property
    def is_little_endian(self):
        return self.byte_order == '1'

    @property
    def c_type(self):
        """Choose the smallest standard C type that fits this signal."""
        if self.is_signed == '-':
            if self.length <= 8:
                return "int8_t"
            elif self.length <= 16:
                return "int16_t"
            else:
                return "int32_t"
        else:
            if self.length <= 8:
                return "uint8_t"
            elif self.length <= 16:
                return "uint16_t"
            else:
                return "uint32_t"

    @property
    def mask(self):
        return (1 << self.length) - 1


class Message:
    """Represents one BO_ entry."""
    def __init__(self, msg_id, name, dlc, sender):
        self.msg_id = int(msg_id)
        self.name = name
        self.dlc = int(dlc)
        self.sender = sender
        self.signals = []
        self.comment = ""
        self.cycle_time_ms = 0

    def add_signal(self, sig):
        self.signals.append(sig)


class DbcFile:
    """Container for a parsed DBC file."""
    def __init__(self):
        self.messages = []
        self.nodes = []
        self.comments = {}

# ---------------------------------------------------------------------------
# Parser
# ---------------------------------------------------------------------------

# Regex patterns for DBC elements
_RE_BO = re.compile(
    r'^BO_\s+(\d+)\s+(\w+)\s*:\s*(\d+)\s+(\w+)')
_RE_SG = re.compile(
    r'^\s+SG_\s+(\w+)\s*:\s*(\d+)\|(\d+)@([01])([+-])'
    r'\s*\(([^,]+),([^)]+)\)\s*\[([^|]+)\|([^\]]+)\]'
    r'\s*"([^"]*)"\s*(.*)')
_RE_BU = re.compile(r'^BU_\s*:\s*(.*)')
_RE_CM_BO = re.compile(r'^CM_\s+BO_\s+(\d+)\s+"([^"]*)"')
_RE_CM_SG = re.compile(r'^CM_\s+SG_\s+(\d+)\s+(\w+)\s+"([^"]*)"')
_RE_BA_CYCLE = re.compile(
    r'^BA_\s+"GenMsgCycleTime"\s+BO_\s+(\d+)\s+(\d+)\s*;')


def parse_dbc(path):
    """Parse a DBC file and return a DbcFile object."""
    dbc = DbcFile()
    current_msg = None

    with open(path, 'r') as f:
        for line in f:
            line = line.rstrip('\n')

            # --- Bus Units (nodes) ---
            m = _RE_BU.match(line)
            if m:
                dbc.nodes = m.group(1).split()
                continue

            # --- Message ---
            m = _RE_BO.match(line)
            if m:
                current_msg = Message(m.group(1), m.group(2),
                                      m.group(3), m.group(4))
                dbc.messages.append(current_msg)
                continue

            # --- Signal ---
            m = _RE_SG.match(line)
            if m and current_msg is not None:
                receivers = [r.strip() for r in m.group(11).split(',')
                             if r.strip()]
                sig = Signal(
                    name=m.group(1),
                    start_bit=m.group(2),
                    length=m.group(3),
                    byte_order=m.group(4),
                    is_signed=m.group(5),
                    factor=m.group(6),
                    offset=m.group(7),
                    minimum=m.group(8),
                    maximum=m.group(9),
                    unit=m.group(10),
                    receivers=receivers,
                )
                current_msg.add_signal(sig)
                continue

            # --- Comment on message ---
            m = _RE_CM_BO.match(line)
            if m:
                msg_id = int(m.group(1))
                for msg in dbc.messages:
                    if msg.msg_id == msg_id:
                        msg.comment = m.group(2)
                continue

            # --- Comment on signal (stored but not emitted) ---
            m = _RE_CM_SG.match(line)
            if m:
                key = (int(m.group(1)), m.group(2))
                dbc.comments[key] = m.group(3)
                continue

            # --- Cycle-time attribute ---
            m = _RE_BA_CYCLE.match(line)
            if m:
                msg_id = int(m.group(1))
                cycle = int(m.group(2))
                for msg in dbc.messages:
                    if msg.msg_id == msg_id:
                        msg.cycle_time_ms = cycle
                continue

    return dbc

# ---------------------------------------------------------------------------
# Code generator helpers
# ---------------------------------------------------------------------------

_HEADER_BANNER = """\
//###########################################################################
//
// FILE:   {filename}
//
// TITLE:  Auto-generated CAN message definitions from DBC
//
// GENERATED BY: tools/dbc_to_c.py
// SOURCE DBC:   {dbc_name}
//
// WARNING: This file is auto-generated. Do not edit manually.
//          Re-run the generator when the DBC file changes.
//
//###########################################################################
"""


def _guard_name(filename):
    return filename.upper().replace('.', '_').replace('/', '_') + '_'


def _define_name(msg_name):
    """Convert CamelCase message name to UPPER_SNAKE for #define."""
    s1 = re.sub(r'(.)([A-Z][a-z]+)', r'\1_\2', msg_name)
    return re.sub(r'([a-z0-9])([A-Z])', r'\1_\2', s1).upper()

# ---------------------------------------------------------------------------
# Header generator
# ---------------------------------------------------------------------------

def generate_header(dbc, dbc_name, node):
    """Return the contents of can_message.h as a string."""
    lines = []
    filename = "can_message.h"
    guard = _guard_name(filename)

    lines.append(_HEADER_BANNER.format(filename=filename, dbc_name=dbc_name))
    lines.append(f"#ifndef {guard}")
    lines.append(f"#define {guard}")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("")

    # ---------- Message ID defines ----------
    lines.append("//")
    lines.append("// CAN Message IDs")
    lines.append("//")
    for msg in dbc.messages:
        dname = "CAN_ID_" + _define_name(msg.name)
        lines.append(f"#define {dname:<40s} (0x{msg.msg_id:03X}U)")
    lines.append("")

    # ---------- DLC defines ----------
    lines.append("//")
    lines.append("// CAN Message DLCs (bytes)")
    lines.append("//")
    for msg in dbc.messages:
        dname = "CAN_DLC_" + _define_name(msg.name)
        lines.append(f"#define {dname:<40s} ({msg.dlc}U)")
    lines.append("")

    # ---------- Cycle-time defines ----------
    lines.append("//")
    lines.append("// CAN Message Cycle Times (ms, 0 = event-triggered)")
    lines.append("//")
    for msg in dbc.messages:
        dname = "CAN_CYCLE_" + _define_name(msg.name)
        lines.append(f"#define {dname:<40s} ({msg.cycle_time_ms}U)")
    lines.append("")

    # ---------- TX / RX classification ----------
    tx_msgs = [m for m in dbc.messages if m.sender == node]
    rx_msgs = [m for m in dbc.messages if m.sender != node]

    lines.append("//")
    lines.append(f"// Number of TX messages (sent by {node})")
    lines.append("//")
    lines.append(f"#define CAN_NUM_TX_MSGS  ({len(tx_msgs)}U)")
    lines.append("")
    lines.append("//")
    lines.append(f"// Number of RX messages (received by {node})")
    lines.append("//")
    lines.append(f"#define CAN_NUM_RX_MSGS  ({len(rx_msgs)}U)")
    lines.append("")

    # ---------- Per-message signal structs ----------
    for msg in dbc.messages:
        struct_name = f"CAN_MSG_{_define_name(msg.name)}_t"
        if msg.comment:
            lines.append(f"// {msg.comment}")
        lines.append(f"typedef struct")
        lines.append("{")
        for sig in msg.signals:
            comment = dbc.comments.get((msg.msg_id, sig.name), "")
            cmt = f"  // {comment}" if comment else ""
            lines.append(f"    {sig.c_type:<12s} {sig.name};{cmt}")
        lines.append(f"}} {struct_name};")
        lines.append("")

    # ---------- Pack / Unpack prototypes ----------
    lines.append("//")
    lines.append("// Pack functions: struct -> raw CAN data bytes")
    lines.append("//")
    for msg in dbc.messages:
        sn = _define_name(msg.name)
        struct_t = f"CAN_MSG_{sn}_t"
        lines.append(
            f"void CAN_Pack_{msg.name}"
            f"(uint16_t data[], const {struct_t} *msg);")
    lines.append("")

    lines.append("//")
    lines.append("// Unpack functions: raw CAN data bytes -> struct")
    lines.append("//")
    for msg in dbc.messages:
        sn = _define_name(msg.name)
        struct_t = f"CAN_MSG_{sn}_t"
        lines.append(
            f"void CAN_Unpack_{msg.name}"
            f"({struct_t} *msg, const uint16_t data[]);")
    lines.append("")

    lines.append(f"#endif // {guard}")
    lines.append("")
    return '\n'.join(lines)

# ---------------------------------------------------------------------------
# Source generator
# ---------------------------------------------------------------------------

def generate_source(dbc, dbc_name, node):
    """Return the contents of can_message.c as a string."""
    lines = []
    filename = "can_message.c"

    lines.append(_HEADER_BANNER.format(filename=filename, dbc_name=dbc_name))
    lines.append("//")
    lines.append("// Included Files")
    lines.append("//")
    lines.append('#include "can_message.h"')
    lines.append("")

    for msg in dbc.messages:
        sn = _define_name(msg.name)
        struct_t = f"CAN_MSG_{sn}_t"

        # ---- Pack ----
        lines.append("//")
        lines.append(f"// Pack {msg.name} (0x{msg.msg_id:03X}) signals "
                      "into raw data array")
        lines.append("//")
        lines.append(
            f"void CAN_Pack_{msg.name}"
            f"(uint16_t data[], const {struct_t} *msg)")
        lines.append("{")
        lines.append(f"    uint16_t i;")
        lines.append(f"    for (i = 0; i < {msg.dlc}U; i++)")
        lines.append( "    {")
        lines.append( "        data[i] = 0U;")
        lines.append( "    }")

        for sig in msg.signals:
            if not sig.is_little_endian:
                lines.append(f"    // {sig.name}: big-endian packing "
                              "not generated (add manually)")
                continue

            byte_pos = sig.start_bit // 8
            bit_in_byte = sig.start_bit % 8
            remaining = sig.length
            src_shift = 0

            lines.append(f"    // {sig.name}: start={sig.start_bit}, "
                          f"len={sig.length}")
            while remaining > 0:
                bits_this_byte = min(remaining, 8 - bit_in_byte)
                mask_val = (1 << bits_this_byte) - 1

                if src_shift == 0:
                    src_expr = f"(uint16_t)msg->{sig.name}"
                else:
                    src_expr = (f"((uint16_t)msg->{sig.name}"
                                f" >> {src_shift}U)")

                lines.append(
                    f"    data[{byte_pos}] |= "
                    f"({src_expr} & 0x{mask_val:02X}U)"
                    f" << {bit_in_byte}U;")

                src_shift += bits_this_byte
                remaining -= bits_this_byte
                byte_pos += 1
                bit_in_byte = 0

        lines.append("}")
        lines.append("")

        # ---- Unpack ----
        lines.append("//")
        lines.append(f"// Unpack {msg.name} (0x{msg.msg_id:03X}) raw data "
                      "into signal struct")
        lines.append("//")
        lines.append(
            f"void CAN_Unpack_{msg.name}"
            f"({struct_t} *msg, const uint16_t data[])")
        lines.append("{")

        for sig in msg.signals:
            if not sig.is_little_endian:
                lines.append(f"    // {sig.name}: big-endian unpacking "
                              "not generated (add manually)")
                continue

            byte_pos = sig.start_bit // 8
            bit_in_byte = sig.start_bit % 8
            remaining = sig.length
            dst_shift = 0

            lines.append(f"    // {sig.name}: start={sig.start_bit}, "
                          f"len={sig.length}")
            lines.append(f"    msg->{sig.name} = 0;")
            while remaining > 0:
                bits_this_byte = min(remaining, 8 - bit_in_byte)
                mask_val = (1 << bits_this_byte) - 1

                extract = (f"(({sig.c_type})(data[{byte_pos}]"
                           f" >> {bit_in_byte}U)"
                           f" & 0x{mask_val:02X}U)")
                if dst_shift > 0:
                    extract = f"({extract} << {dst_shift}U)"

                lines.append(
                    f"    msg->{sig.name} |= {extract};")

                dst_shift += bits_this_byte
                remaining -= bits_this_byte
                byte_pos += 1
                bit_in_byte = 0

        lines.append("}")
        lines.append("")

    lines.append("//")
    lines.append("// End of File")
    lines.append("//")
    lines.append("")
    return '\n'.join(lines)

# ---------------------------------------------------------------------------
# Main entry point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Convert a DBC file to C source for TI F280039C CAN driver")
    parser.add_argument("dbc", help="Path to the .dbc file")
    parser.add_argument("--outdir", default=".",
                        help="Output directory for generated files")
    parser.add_argument("--node", default="ECU",
                        help="This ECU node name in the DBC (default: ECU)")
    args = parser.parse_args()

    dbc = parse_dbc(args.dbc)
    dbc_name = os.path.basename(args.dbc)

    header = generate_header(dbc, dbc_name, args.node)
    source = generate_source(dbc, dbc_name, args.node)

    os.makedirs(args.outdir, exist_ok=True)

    h_path = os.path.join(args.outdir, "can_message.h")
    c_path = os.path.join(args.outdir, "can_message.c")

    with open(h_path, 'w') as f:
        f.write(header)
    with open(c_path, 'w') as f:
        f.write(source)

    print(f"Generated {h_path}")
    print(f"Generated {c_path}")

    # Summary
    tx = [m for m in dbc.messages if m.sender == args.node]
    rx = [m for m in dbc.messages if m.sender != args.node]
    print(f"\nDBC summary ({dbc_name}):")
    print(f"  Nodes   : {', '.join(dbc.nodes)}")
    print(f"  Messages: {len(dbc.messages)}")
    print(f"  TX (from {args.node}): {len(tx)}")
    for m in tx:
        print(f"    0x{m.msg_id:03X} {m.name} ({m.dlc}B, "
              f"{m.cycle_time_ms}ms)")
    print(f"  RX (to {args.node}):   {len(rx)}")
    for m in rx:
        print(f"    0x{m.msg_id:03X} {m.name} ({m.dlc}B, "
              f"{m.cycle_time_ms}ms)")


if __name__ == "__main__":
    main()

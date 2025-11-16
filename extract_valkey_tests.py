#!/usr/bin/env python3
"""
Extract test commands from Valkey repository and compare RESP vs RESPB

This script parses Valkey test files (.tcl) to extract command examples,
converts them to both RESP and RESPB formats, and compares efficiency.
"""

import re
import os
import sys
from pathlib import Path
from typing import List, Tuple
from respb_converter import RESPParser, RESPBSerializer, ProtocolComparator, RESPCommand


class ValkeyTestExtractor:
    """Extract Redis/Valkey commands from TCL test files."""
    
    def __init__(self, valkey_path: str = "/tmp/valkey"):
        self.valkey_path = Path(valkey_path)
        self.test_dir = self.valkey_path / "tests" / "unit" / "type"
    
    def extract_commands_from_tcl(self, file_path: Path) -> List[Tuple[str, List[str]]]:
        """Extract commands from a TCL test file."""
        commands = []
        
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
        except:
            return commands
        
        # Match patterns like: r set key value
        # Pattern: r <command> <args...>
        pattern = r'\br\s+(\w+)\s+([^\n\}]+)'
        
        matches = re.findall(pattern, content)
        
        for cmd, args_str in matches:
            # Skip some TCL control commands
            if cmd.lower() in ['test', 'start_server', 'tags', 'assert', 'if', 
                              'for', 'while', 'set', 'incr', 'puts', 'return',
                              'catch', 'wait', 'lindex', 'llength', 'lrange',
                              'expr', 'string', 'split', 'join', 'foreach']:
                continue
            
            # Parse arguments
            args = []
            # Handle both quoted and unquoted arguments
            arg_pattern = r'(?:"([^"]*)"|\'([^\']*)\'|(\{[^\}]*\})|(\S+))'
            arg_matches = re.findall(arg_pattern, args_str)
            
            for match in arg_matches:
                # Get the non-empty group
                arg = next((g for g in match if g), '')
                if arg and arg.strip():
                    # Remove TCL braces if present
                    arg = arg.strip('{}')
                    args.append(arg)
            
            if args:
                commands.append((cmd.upper(), args))
        
        return commands
    
    def extract_from_all_tests(self) -> List[Tuple[str, List[str]]]:
        """Extract commands from all test files."""
        all_commands = []
        
        if not self.test_dir.exists():
            print(f"Warning: Test directory not found: {self.test_dir}")
            return all_commands
        
        for tcl_file in self.test_dir.glob("*.tcl"):
            print(f"Extracting from {tcl_file.name}...")
            commands = self.extract_commands_from_tcl(tcl_file)
            all_commands.extend(commands)
        
        return all_commands
    
    @staticmethod
    def command_to_resp(command: str, args: List[str]) -> bytes:
        """Convert a command and args to RESP format."""
        parts = [command] + args
        resp = f"*{len(parts)}\r\n".encode()
        
        for part in parts:
            part_bytes = part.encode('utf-8')
            resp += f"${len(part_bytes)}\r\n".encode()
            resp += part_bytes + b"\r\n"
        
        return resp


def generate_test_report(output_file: str = "respb_comparison_report.md"):
    """Generate a comprehensive comparison report."""
    
    print("=" * 80)
    print("RESPB Protocol Comparison Generator")
    print("=" * 80)
    print()
    
    extractor = ValkeyTestExtractor()
    parser = RESPParser()
    serializer = RESPBSerializer()
    comparator = ProtocolComparator()
    
    # Extract commands from Valkey tests
    print("Extracting commands from Valkey test files...")
    test_commands = extractor.extract_from_all_tests()
    print(f"Found {len(test_commands)} command examples\n")
    
    # Also add some manually crafted examples for comprehensive coverage
    manual_examples = [
        ("GET", ["mykey"]),
        ("SET", ["mykey", "value"]),
        ("SET", ["key", "value", "EX", "60"]),
        ("SET", ["key", "value", "NX", "EX", "30"]),
        ("MGET", ["key1", "key2", "key3"]),
        ("MSET", ["k1", "v1", "k2", "v2", "k3", "v3"]),
        ("DEL", ["key1", "key2", "key3"]),
        ("LPUSH", ["mylist", "a", "b", "c"]),
        ("RPUSH", ["mylist", "x", "y", "z"]),
        ("LRANGE", ["mylist", "0", "-1"]),
        ("SADD", ["myset", "member1", "member2", "member3"]),
        ("SINTER", ["set1", "set2", "set3"]),
        ("ZADD", ["myzset", "1.0", "one", "2.0", "two", "3.0", "three"]),
        ("ZRANGE", ["myzset", "0", "-1", "WITHSCORES"]),
        ("HSET", ["myhash", "field1", "value1", "field2", "value2"]),
        ("HMGET", ["myhash", "field1", "field2", "field3"]),
        ("PUBLISH", ["channel", "message"]),
        ("SUBSCRIBE", ["channel1", "channel2"]),
        ("PING", []),
        ("ECHO", ["hello world"]),
        ("INCR", ["counter"]),
        ("INCRBY", ["counter", "5"]),
        ("EXPIRE", ["mykey", "60"]),
        ("TTL", ["mykey"]),
    ]
    
    print(f"Adding {len(manual_examples)} manual test examples\n")
    test_commands.extend(manual_examples)
    
    # Process commands and collect results
    results = []
    errors = []
    command_types = {}
    
    print("Processing commands and generating comparisons...")
    for cmd, args in test_commands[:500]:  # Limit to 500 for performance
        try:
            # Convert to RESP
            resp_data = extractor.command_to_resp(cmd, args)
            
            # Parse and convert to RESPB
            parsed_cmd = parser.parse_command(resp_data)
            respb_data = serializer.serialize(parsed_cmd)
            
            # Compare
            args_preview = ' '.join(args[:3])[:50]
            if len(args) > 3:
                args_preview += "..."
            
            result = comparator.compare_command(
                resp_data, respb_data,
                f"{cmd} {args_preview}"
            )
            results.append(result)
            
            # Track by command type
            if cmd not in command_types:
                command_types[cmd] = []
            command_types[cmd].append(result)
            
        except Exception as e:
            errors.append((cmd, args, str(e)))
    
    # Generate report
    print(f"\nGenerating report to {output_file}...")
    
    with open(output_file, 'w') as f:
        f.write("# RESPB Protocol Comparison Report\n\n")
        f.write("This report compares the RESP (text) and RESPB (binary) protocols ")
        f.write("using real command examples from Valkey tests.\n\n")
        
        f.write("## Executive Summary\n\n")
        
        total_resp = sum(r['resp_size'] for r in results)
        total_respb = sum(r['respb_size'] for r in results)
        total_savings = total_resp - total_respb
        
        f.write(f"- **Commands analyzed:** {len(results)}\n")
        f.write(f"- **Total RESP size:** {total_resp:,} bytes\n")
        f.write(f"- **Total RESPB size:** {total_respb:,} bytes\n")
        f.write(f"- **Total savings:** {total_savings:,} bytes ({total_savings/total_resp*100:.1f}%)\n")
        f.write(f"- **Average RESP size:** {total_resp/len(results):.1f} bytes\n")
        f.write(f"- **Average RESPB size:** {total_respb/len(results):.1f} bytes\n")
        f.write(f"- **Average savings:** {total_savings/len(results):.1f} bytes per command\n\n")
        
        # Breakdown by command type
        f.write("## Savings by Command Type\n\n")
        f.write("| Command | Count | Avg RESP | Avg RESPB | Avg Savings | Savings % |\n")
        f.write("|---------|-------|----------|-----------|-------------|----------|\n")
        
        for cmd in sorted(command_types.keys()):
            cmd_results = command_types[cmd]
            avg_resp = sum(r['resp_size'] for r in cmd_results) / len(cmd_results)
            avg_respb = sum(r['respb_size'] for r in cmd_results) / len(cmd_results)
            avg_savings = avg_resp - avg_respb
            savings_pct = (avg_savings / avg_resp * 100) if avg_resp > 0 else 0
            
            f.write(f"| {cmd:12s} | {len(cmd_results):5d} | {avg_resp:8.1f} | "
                   f"{avg_respb:9.1f} | {avg_savings:11.1f} | {savings_pct:7.1f}% |\n")
        
        f.write("\n## Top 20 Best Savings Examples\n\n")
        f.write("| Command | RESP | RESPB | Savings | % |\n")
        f.write("|---------|------|-------|---------|---|\n")
        
        sorted_results = sorted(results, key=lambda x: x['savings_bytes'], reverse=True)[:20]
        for r in sorted_results:
            cmd_short = r['command'][:50]
            f.write(f"| {cmd_short:50s} | {r['resp_size']:4d} | {r['respb_size']:5d} | "
                   f"{r['savings_bytes']:7d} | {r['savings_percent']:5.1f}% |\n")
        
        f.write("\n## Example Comparisons\n\n")
        
        # Show detailed examples for different command types
        example_cmds = ['GET', 'SET', 'MGET', 'MSET', 'LPUSH', 'SADD', 'ZADD', 'HSET', 'PUBLISH']
        for cmd in example_cmds:
            if cmd in command_types and command_types[cmd]:
                f.write(f"### {cmd} Command\n\n")
                example = command_types[cmd][0]
                f.write(f"**Command:** `{example['command']}`\n\n")
                f.write(f"**RESP format ({example['resp_size']} bytes):**\n")
                f.write(f"```\n{example['resp_hex']}\n```\n\n")
                f.write(f"**RESPB format ({example['respb_size']} bytes):**\n")
                f.write(f"```\n{example['respb_hex']}\n```\n\n")
                f.write(f"**Savings:** {example['savings_bytes']} bytes ({example['savings_percent']:.1f}%)\n\n")
        
        if errors:
            f.write("\n## Errors Encountered\n\n")
            f.write(f"Total errors: {len(errors)}\n\n")
            for cmd, args, error in errors[:20]:
                f.write(f"- **{cmd}** {args[:3]}: {error}\n")
    
    print(f"\n✓ Report generated: {output_file}")
    print(f"✓ Analyzed {len(results)} commands")
    print(f"✓ Total savings: {total_savings:,} bytes ({total_savings/total_resp*100:.1f}%)")
    
    return results


def main():
    """Main function."""
    # Generate comprehensive report
    results = generate_test_report("respb_comparison_report.md")
    
    print("\n" + "=" * 80)
    print("Done! Check respb_comparison_report.md for detailed analysis.")
    print("=" * 80)


if __name__ == '__main__':
    main()


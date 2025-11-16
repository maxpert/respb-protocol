#!/usr/bin/env python3
"""
Valkey Commands Scraper

Scrapes all commands from https://valkey.io/commands/ and exports them to a CSV file.
"""

import csv
import sys
import requests
from bs4 import BeautifulSoup
from typing import List, Dict


def fetch_command_details(command_name: str, base_url: str = "https://valkey.io/commands/") -> str:
    """
    Fetch the command format from the individual command page.
    
    Args:
        command_name: The command name (e.g., "GET", "SET")
        base_url: The base URL for command pages
    
    Returns:
        Command format/syntax string
    """
    # Convert command name to URL format (lowercase with hyphens)
    url_name = command_name.lower().replace(' ', '-').replace('_', '-').replace('.', '-')
    command_url = f"{base_url}{url_name}/"
    
    try:
        response = requests.get(command_url, timeout=10)
        response.raise_for_status()
        
        soup = BeautifulSoup(response.content, 'lxml')
        
        # Strategy 1: Look for "Syntax" heading and get the next pre/code element
        all_text = soup.get_text()
        if 'Syntax' in all_text or 'syntax' in all_text:
            headings = soup.find_all(['h1', 'h2', 'h3', 'h4'])
            for heading in headings:
                heading_text = heading.get_text(strip=True)
                if heading_text.lower() == 'syntax':
                    # Look for the next sibling that contains code
                    sibling = heading.find_next_sibling()
                    attempts = 0
                    while sibling and attempts < 5:
                        # Check if this element or its children contain pre or code
                        code_elem = sibling.find(['pre', 'code'])
                        if code_elem:
                            syntax = code_elem.get_text(strip=True)
                            # Clean the syntax
                            syntax = syntax.replace('\n', ' ').strip()
                            # Remove any leading prompts
                            if syntax.startswith('>'):
                                syntax = syntax[1:].strip()
                            # Validate it looks like a command
                            if syntax.upper().startswith(command_name.upper().split()[0]) and len(syntax) < 300:
                                return syntax
                        sibling = sibling.find_next_sibling()
                        attempts += 1
        
        # Strategy 2: Look for pre/code blocks that contain the command syntax pattern
        # Command syntax usually has brackets [ ] or uppercase command names
        pre_blocks = soup.find_all('pre')
        for pre in pre_blocks:
            text = pre.get_text(strip=True)
            # Remove newlines and normalize spaces
            text = ' '.join(text.split())
            
            # Skip if it contains example output indicators
            if '127.0.0.1' in text or text.startswith('(integer)') or text.startswith('OK'):
                continue
            
            # Check if it starts with the command name (case insensitive)
            if text.upper().startswith(command_name.upper().split()[0]):
                # Check if it looks like syntax (contains brackets or just command + args)
                if '[' in text or len(text.split()) <= 15:
                    # Remove any prompt characters
                    text = text.lstrip('>')
                    if len(text) < 300:
                        return text.strip()
        
        # Strategy 3: Look in code tags
        code_blocks = soup.find_all('code')
        for code in code_blocks:
            text = code.get_text(strip=True)
            # Remove newlines and normalize
            text = ' '.join(text.split())
            
            # Skip example outputs
            if '127.0.0.1' in text or text.startswith('(integer)') or text.startswith('OK'):
                continue
                
            if text.upper().startswith(command_name.upper().split()[0]):
                # Looks like it could be syntax
                if '[' in text or (len(text.split()) <= 15 and not '=' in text):
                    if len(text) < 300:
                        return text.strip()
    
    except Exception as e:
        pass
    
    # Return the command name as fallback
    return command_name


def scrape_valkey_commands(url: str = "https://valkey.io/commands/") -> List[Dict[str, str]]:
    """
    Scrape Valkey commands from the commands page.
    
    Args:
        url: The URL to scrape (default: https://valkey.io/commands/)
    
    Returns:
        List of dictionaries containing command information
    """
    print(f"Fetching data from {url}...")
    
    try:
        response = requests.get(url, timeout=30)
        response.raise_for_status()
    except requests.RequestException as e:
        print(f"Error fetching URL: {e}")
        sys.exit(1)
    
    print("Parsing HTML content...")
    soup = BeautifulSoup(response.content, 'lxml')
    
    commands = []
    current_category = "General"
    
    # The page structure has commands in <code> tags within various elements
    # Try multiple approaches to find commands
    
    # Approach 1: Find all <code> tags that look like commands (uppercase)
    code_tags = soup.find_all('code')
    
    for code_tag in code_tags:
        command_text = code_tag.get_text(strip=True)
        
        # Valkey commands are typically uppercase and contain only letters, dots, underscores, and hyphens
        # Skip if it doesn't look like a command name
        if not command_text or len(command_text) > 100:
            continue
        
        # Get the parent element to find the description
        parent = code_tag.parent
        if parent:
            # Try to get the full text and extract description
            full_text = parent.get_text(strip=True)
            # Remove the command name from the text to get description
            description = full_text.replace(command_text, '', 1).strip()
            
            # Check if there's a heading above for category
            # Look backwards through siblings to find h2
            check_element = parent
            for _ in range(10):  # Look back up to 10 elements
                prev = check_element.find_previous(['h2', 'h3'])
                if prev:
                    category_text = prev.get_text(strip=True)
                    if category_text and len(category_text) < 100:
                        current_category = category_text
                    break
                check_element = check_element.parent if check_element.parent else check_element
            
            # Only add if we have both command and description
            if command_text and description:
                commands.append({
                    'command': command_text,
                    'usage': '',  # Will be filled in later
                    'category': current_category,
                    'description': description
                })
    
    # Deduplicate commands (keep first occurrence)
    seen = set()
    unique_commands = []
    for cmd in commands:
        key = cmd['command']
        if key not in seen:
            seen.add(key)
            unique_commands.append(cmd)
    
    print(f"Found {len(unique_commands)} unique commands")
    
    # Fetch command usage/formats (this may take a while)
    print("Fetching command usage from individual pages...")
    for i, cmd in enumerate(unique_commands):
        if (i + 1) % 50 == 0:
            print(f"  Progress: {i + 1}/{len(unique_commands)} commands...")
        
        usage = fetch_command_details(cmd['command'])
        cmd['usage'] = usage
    
    print("Finished fetching command usage.")
    return unique_commands


def export_to_csv(commands: List[Dict[str, str]], output_file: str = "valkey_commands.csv"):
    """
    Export commands to a CSV file.
    
    Args:
        commands: List of command dictionaries
        output_file: Output CSV filename
    """
    if not commands:
        print("No commands to export!")
        return
    
    print(f"Exporting to {output_file}...")
    
    try:
        with open(output_file, 'w', newline='', encoding='utf-8') as csvfile:
            fieldnames = ['command', 'usage', 'category', 'description']
            writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
            
            writer.writeheader()
            for command in commands:
                writer.writerow(command)
        
        print(f"Successfully exported {len(commands)} commands to {output_file}")
    except IOError as e:
        print(f"Error writing to CSV file: {e}")
        sys.exit(1)


def main():
    """Main function to run the scraper."""
    print("=" * 60)
    print("Valkey Commands Scraper")
    print("=" * 60)
    print()
    
    # Scrape the commands
    commands = scrape_valkey_commands()
    
    # Export to CSV
    if commands:
        export_to_csv(commands)
        print()
        print("Done! You can now open 'valkey_commands.csv' to view the results.")
    else:
        print("Warning: No commands were found. The page structure may have changed.")
        sys.exit(1)


if __name__ == "__main__":
    main()


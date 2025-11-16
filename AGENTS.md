# Agent Rules for RESPB-Valkey Repository

This document defines rules and guidelines for AI agents working on this repository.

## Writing Style Guidelines

All documentation and comments must follow these rules:

### Required Style

- Use clear, simple language
- Be spartan and informative
- Use short, impactful sentences
- Use active voice. Avoid passive voice
- Focus on practical, actionable insights
- Use data and examples to support claims when possible
- Use "you" and "your" to directly address the reader
- Double check your claims, and if possible do maths with scripts or whatever to not make errors.

### Prohibited Style Elements

- Do not use em dashes anywhere. Use only commas, periods, or other standard punctuation
- Do not use constructions like "not just this, but also this"
- Do not use metaphors and clichés
- Do not use generalizations
- Do not use common setup language including: in conclusion, in closing, etc.
- Do not use unnecessary or excessive adjectives and adverbs
- Do not use hashtags
- Do not use semicolons
- Do not use markdown formatting in prose (no asterisks for bold, no emphasis markers)
- Do not use asterisks for formatting

### Code Formatting in Documentation

- Use backticks for all code elements: commands, opcodes, format strings, file paths
- Use backticks for RESP format strings: `*2\r\n$3\r\nGET\r\n`
- Use backticks for binary formats: `[0x0001][0x0000][0x0003]`
- Use backticks for command examples: `GET key`, `SET foo bar`
- Use backticks for file names and paths: `respb-specs.md`, `respb_converter.py`

## Code Style

- NO TODO COMMENTS
- NO BOGUS TEST CASES
- NO SHORTCUTS TO MAKE TEST PASS

### No junk scripts or left overs
- If there is existing script/file that is doing the same function as what youa are doing reuse it
- Only create scripts for repeated tasks not one time
- If you have to create script for one time tasks, delete it immediately
- No additional md files unless asked for. Plans/Report/Achievements should stay out of md files unless asked for.

### Python

- Follow PEP 8 style guidelines
- Use type hints where appropriate
- Keep functions focused and single-purpose
- Add docstrings for public functions and classes
- Use meaningful variable names

### C

- Follow existing code style in protocol-bench/
- Use consistent indentation (spaces, not tabs)
- Add comments for complex logic
- Follow Valkey/Redis coding conventions where applicable
- Clone repos to /tmp if you want to read code or investigate something

## File Organization

### Directory Structure

- Keep Mendeley-related files in `mendeley/` directory
- Keep benchmark code in `protocol-bench/` directory
- Keep documentation in root directory
- Keep Python tools in root directory

### File Naming

- Use lowercase with underscores for Python files: `respb_converter.py`
- Use lowercase with hyphens for markdown files: `respb-specs.md`
- Use descriptive names that indicate purpose

## Git Practices

### Commits

- Write clear, descriptive commit messages
- Group related changes together
- Squash commits when appropriate (user will handle force push)
- Do not commit large data files or generated artifacts

### Files to Ignore

- Large AOF files (`*.aof`, `*.aof.*`)
- Dataset zip files (`*.zip` in mendeley/)
- Generated data directories (`mendeley/data/`, `mendeley/redis_data/`)
- Build artifacts and temporary files

## Documentation Standards

### Markdown Files

- Use proper heading hierarchy
- Keep sections focused and concise
- Use code blocks with language tags for examples
- Use tables for structured data
- Keep line length reasonable (80-100 characters)

### Code Comments

- Explain why, not what
- Keep comments up to date with code changes
- Remove commented-out code before committing

## Protocol Specification Rules

### Opcode References

- Always use hexadecimal format with 0x prefix: `0x0001`, `0xF000`
- Be consistent with opcode ranges and allocations
- Reference `respb-commands.md` for opcode mappings

### Format Descriptions

- Use clear notation for binary formats: `[2B keylen][key]`
- Show RESP format with backticks: `` `*2\r\n$3\r\nGET\r\n` ``
- Use consistent terminology (mux ID, opcode, payload, etc.)

## Testing Requirements

### Before Committing

- Ensure code runs without errors
- Verify tests pass if test suite exists
- Check for linter errors
- Verify documentation renders correctly

### Benchmark Code

- Maintain compatibility with Valkey's production parser
- Keep benchmark results accurate and reproducible
- Document any assumptions or limitations

## Error Handling

### User Communication

- Be direct and clear about errors
- Provide actionable solutions
- Avoid technical jargon when possible
- Use examples to illustrate problems

### Code Errors

- Handle edge cases gracefully
- Provide meaningful error messages
- Log errors appropriately
- Do not silently fail

## Performance Considerations

### Optimization

- Profile before optimizing
- Document performance improvements with data
- Maintain readability over micro-optimizations
- Consider memory usage alongside CPU usage

### Benchmarking

- Use real production code for comparisons
- Report accurate numbers with context
- Explain methodology clearly
- Acknowledge limitations

## Extensibility

### Adding New Features

- Follow existing patterns
- Update documentation simultaneously
- Consider backward compatibility
- Maintain opcode space organization

### Module Support

- Use module opcode 0xF000 for module commands
- Document module ID allocations
- Support RESP passthrough for unknown commands
- Provide fallback mechanisms

## Review Checklist

Before submitting changes, verify:

- [ ] Writing style follows all guidelines
- [ ] Code is properly formatted
- [ ] Documentation is updated
- [ ] No linter errors
- [ ] Tests pass (if applicable)
- [ ] Commit message is clear
- [ ] No large files committed
- [ ] Code examples use backticks
- [ ] No em dashes or semicolons in prose
- [ ] Active voice used throughout

## Questions or Clarifications

When in doubt:
- Follow existing patterns in the codebase
- Prefer simplicity over cleverness
- Ask for clarification rather than guessing
- Maintain consistency with established conventions


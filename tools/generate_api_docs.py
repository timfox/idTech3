#!/usr/bin/env python3
"""
API Documentation Generator for id Tech 3

This script extracts documentation from C/C++ source files and generates
comprehensive API reference documentation.
"""

import os
import re
import sys
from typing import Dict, List, Optional, Tuple
from dataclasses import dataclass
from pathlib import Path

@dataclass
class FunctionDoc:
    """Represents a documented function"""
    name: str
    signature: str
    description: str
    params: Dict[str, str]
    returns: str
    file: str
    line: int
    category: str = ""

@dataclass
class StructDoc:
    """Represents a documented struct/union"""
    name: str
    description: str
    members: Dict[str, str]
    file: str
    line: int

@dataclass
class MacroDoc:
    """Represents a documented macro/define"""
    name: str
    value: str
    description: str
    file: str
    line: int

class APIDocumentationGenerator:
    """Main API documentation generator"""

    def __init__(self, source_dirs: List[str]):
        self.source_dirs = source_dirs
        self.functions: List[FunctionDoc] = []
        self.structs: List[StructDoc] = []
        self.macros: List[MacroDoc] = []
        self.categories = {
            'common': 'Common Utilities',
            'client': 'Client System',
            'server': 'Server System',
            'renderer': 'Rendering Engine',
            'sound': 'Audio System',
            'game': 'Game Logic',
            'cgame': 'Client Game',
            'ui': 'User Interface'
        }

    def extract_function_docs(self, content: str, file_path: str) -> None:
        """Extract function documentation from source content"""

        # Pattern for function documentation blocks
        func_pattern = r'/\*\*\s*\n(?:\s*\*?\s*@brief\s*(.*?)\n)?(?:\s*\*?\s*@param\s+(\w+)\s*(.*?)\n)*?(?:\s*\*?\s*@return\s*(.*?)\n)?\s*\*/\s*\n(?:static\s+|inline\s+|extern\s+)*([^(\n]+)\s*\(([^)]*)\)\s*;?'

        for match in re.finditer(func_pattern, content, re.MULTILINE | re.DOTALL):
            brief = match.group(1) or ""
            return_desc = match.group(4) or ""

            # Extract parameters from @param tags
            params = {}
            param_matches = re.findall(r'@param\s+(\w+)\s*(.*?)(?=@|\*/|$)', match.string[match.start():match.end()], re.DOTALL)
            for param_name, param_desc in param_matches:
                params[param_name.strip()] = param_desc.strip()

            func_name = match.group(5).strip()
            signature = f"{match.group(5).strip()}({match.group(6).strip()})"

            # Determine category from file path
            category = "unknown"
            for cat, desc in self.categories.items():
                if cat in file_path.lower():
                    category = cat
                    break

            # Find line number
            line_num = content[:match.start()].count('\n') + 1

            func_doc = FunctionDoc(
                name=func_name,
                signature=signature,
                description=brief.strip(),
                params=params,
                returns=return_desc.strip(),
                file=file_path,
                line=line_num,
                category=category
            )

            self.functions.append(func_doc)

    def extract_struct_docs(self, content: str, file_path: str) -> None:
        """Extract struct/union documentation"""

        # Pattern for struct documentation
        struct_pattern = r'/\*\*\s*\n\s*\*?\s*(.*?)\n\s*\*/\s*\n(?:typedef\s+)?(?:struct|union)\s+(\w+)\s*\{([^}]*)\}'

        for match in re.finditer(struct_pattern, content, re.MULTILINE | re.DOTALL):
            desc = match.group(1).strip()
            name = match.group(2).strip()
            members_text = match.group(3)

            # Parse members
            members = {}
            member_lines = members_text.split('\n')
            for line in member_lines:
                line = line.strip()
                if line and not line.startswith('//') and not line.startswith('/*'):
                    # Simple member extraction - could be enhanced
                    parts = line.replace(';', '').split()
                    if len(parts) >= 2:
                        member_name = parts[-1]
                        member_type = ' '.join(parts[:-1])
                        members[member_name] = member_type

            line_num = content[:match.start()].count('\n') + 1

            struct_doc = StructDoc(
                name=name,
                description=desc,
                members=members,
                file=file_path,
                line=line_num
            )

            self.structs.append(struct_doc)

    def extract_macro_docs(self, content: str, file_path: str) -> None:
        """Extract macro documentation"""

        # Pattern for macro documentation
        macro_pattern = r'/\*\*\s*\n\s*\*?\s*(.*?)\n\s*\*/\s*\n#define\s+(\w+)\s*(.*?)(?:\n|$)'

        for match in re.finditer(macro_pattern, content, re.MULTILINE):
            desc = match.group(1).strip()
            name = match.group(2).strip()
            value = match.group(3).strip()

            line_num = content[:match.start()].count('\n') + 1

            macro_doc = MacroDoc(
                name=name,
                value=value,
                description=desc,
                file=file_path,
                line=line_num
            )

            self.macros.append(macro_doc)

    def process_file(self, file_path: str) -> None:
        """Process a single source file"""
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()

            self.extract_function_docs(content, file_path)
            self.extract_struct_docs(content, file_path)
            self.extract_macro_docs(content, file_path)

        except Exception as e:
            print(f"Warning: Could not process {file_path}: {e}")

    def process_sources(self) -> None:
        """Process all source files in the specified directories"""
        for source_dir in self.source_dirs:
            if not os.path.exists(source_dir):
                print(f"Warning: Source directory {source_dir} does not exist")
                continue

            for root, dirs, files in os.walk(source_dir):
                for file in files:
                    if file.endswith(('.c', '.h', '.cpp', '.hpp')):
                        file_path = os.path.join(root, file)
                        print(f"Processing {file_path}")
                        self.process_file(file_path)

    def generate_markdown_docs(self, output_dir: str) -> None:
        """Generate Markdown documentation"""
        os.makedirs(output_dir, exist_ok=True)

        # Generate index
        self._generate_index(output_dir)

        # Generate function documentation
        self._generate_function_docs(output_dir)

        # Generate struct documentation
        self._generate_struct_docs(output_dir)

        # Generate macro documentation
        self._generate_macro_docs(output_dir)

    def _generate_index(self, output_dir: str) -> None:
        """Generate main index page"""
        index_path = os.path.join(output_dir, 'README.md')

        with open(index_path, 'w') as f:
            f.write("# id Tech 3 API Documentation\n\n")
            f.write("Comprehensive API reference for the id Tech 3 engine.\n\n")

            f.write("## Overview\n\n")
            f.write(f"- **Functions**: {len(self.functions)}\n")
            f.write(f"- **Structures**: {len(self.structs)}\n")
            f.write(f"- **Macros**: {len(self.macros)}\n\n")

            f.write("## Categories\n\n")
            for cat, desc in self.categories.items():
                func_count = len([f for f in self.functions if f.category == cat])
                if func_count > 0:
                    f.write(f"- [{desc}]({cat}.md) ({func_count} functions)\n")
            f.write("\n")

            f.write("## Sections\n\n")
            f.write("- [Functions](functions.md) - All documented functions\n")
            f.write("- [Structures](structs.md) - Data structures and types\n")
            f.write("- [Macros](macros.md) - Preprocessor macros and constants\n")
            f.write("- [Index](index.md) - Alphabetical index\n\n")

            f.write("## Generation\n\n")
            f.write("This documentation was automatically generated from source code comments.\n")
            f.write("Use the following format for documentation:\n\n")
            f.write("```c\n")
            f.write("/**\n")
            f.write(" * @brief Brief description of the function\n")
            f.write(" * @param param1 Description of parameter 1\n")
            f.write(" * @param param2 Description of parameter 2\n")
            f.write(" * @return Description of return value\n")
            f.write(" */\n")
            f.write("void myFunction(int param1, char* param2);\n")
            f.write("```\n")

    def _generate_function_docs(self, output_dir: str) -> None:
        """Generate function documentation"""

        # Group functions by category
        by_category = {}
        for func in self.functions:
            cat = func.category
            if cat not in by_category:
                by_category[cat] = []
            by_category[cat].append(func)

        # Generate main functions page
        functions_path = os.path.join(output_dir, 'functions.md')
        with open(functions_path, 'w') as f:
            f.write("# Functions\n\n")

            for cat, funcs in by_category.items():
                cat_name = self.categories.get(cat, cat.title())
                f.write(f"## {cat_name}\n\n")

                for func in sorted(funcs, key=lambda x: x.name):
                    f.write(f"### {func.name}\n\n")
                    f.write(f"```c\n{func.signature}\n```\n\n")

                    if func.description:
                        f.write(f"{func.description}\n\n")

                    if func.params:
                        f.write("**Parameters:**\n\n")
                        for param, desc in func.params.items():
                            f.write(f"- `{param}`: {desc}\n")
                        f.write("\n")

                    if func.returns:
                        f.write(f"**Returns:** {func.returns}\n\n")

                    f.write(f"**Location:** {func.file}:{func.line}\n\n---\n\n")

        # Generate category-specific pages
        for cat, funcs in by_category.items():
            cat_path = os.path.join(output_dir, f'{cat}.md')
            with open(cat_path, 'w') as f:
                cat_name = self.categories.get(cat, cat.title())
                f.write(f"# {cat_name} API\n\n")

                for func in sorted(funcs, key=lambda x: x.name):
                    f.write(f"## {func.name}\n\n")
                    f.write(f"```c\n{func.signature}\n```\n\n")

                    if func.description:
                        f.write(f"{func.description}\n\n")

                    if func.params:
                        f.write("### Parameters\n\n")
                        for param, desc in func.params.items():
                            f.write(f"- **{param}**: {desc}\n")
                        f.write("\n")

                    if func.returns:
                        f.write(f"### Returns\n\n{func.returns}\n\n")

                    f.write(f"### Location\n\n`{func.file}:{func.line}`\n\n")

    def _generate_struct_docs(self, output_dir: str) -> None:
        """Generate structure documentation"""
        structs_path = os.path.join(output_dir, 'structs.md')
        with open(structs_path, 'w') as f:
            f.write("# Data Structures\n\n")

            for struct in sorted(self.structs, key=lambda x: x.name):
                f.write(f"## {struct.name}\n\n")

                if struct.description:
                    f.write(f"{struct.description}\n\n")

                if struct.members:
                    f.write("### Members\n\n")
                    f.write("| Type | Name | Description |\n")
                    f.write("|------|------|-------------|\n")

                    for name, type_desc in struct.members.items():
                        f.write(f"| `{type_desc}` | `{name}` | |\n")

                    f.write("\n")

                f.write(f"**Location:** {struct.file}:{struct.line}\n\n---\n\n")

    def _generate_macro_docs(self, output_dir: str) -> None:
        """Generate macro documentation"""
        macros_path = os.path.join(output_dir, 'macros.md')
        with open(macros_path, 'w') as f:
            f.write("# Macros and Constants\n\n")

            # Group by category
            by_category = {}
            for macro in self.macros:
                cat = "unknown"
                for c in self.categories:
                    if c in macro.file.lower():
                        cat = c
                        break
                if cat not in by_category:
                    by_category[cat] = []
                by_category[cat].append(macro)

            for cat, macros in by_category.items():
                cat_name = self.categories.get(cat, cat.title())
                f.write(f"## {cat_name}\n\n")

                for macro in sorted(macros, key=lambda x: x.name):
                    f.write(f"### {macro.name}\n\n")
                    f.write(f"```c\n#define {macro.name} {macro.value}\n```\n\n")

                    if macro.description:
                        f.write(f"{macro.description}\n\n")

                    f.write(f"**Location:** {macro.file}:{macro.line}\n\n")

    def generate_index(self, output_dir: str) -> None:
        """Generate alphabetical index"""
        index_path = os.path.join(output_dir, 'index.md')

        with open(index_path, 'w') as f:
            f.write("# Alphabetical Index\n\n")

            # Functions
            f.write("## Functions\n\n")
            for func in sorted(self.functions, key=lambda x: x.name.lower()):
                f.write(f"- [{func.name}](functions.md#{func.name.lower()}) - {func.description[:50]}{'...' if len(func.description) > 50 else ''}\n")
            f.write("\n")

            # Structures
            f.write("## Structures\n\n")
            for struct in sorted(self.structs, key=lambda x: x.name.lower()):
                f.write(f"- [{struct.name}](structs.md#{struct.name.lower()}) - {struct.description[:50]}{'...' if len(struct.description) > 50 else ''}\n")
            f.write("\n")

            # Macros
            f.write("## Macros\n\n")
            for macro in sorted(self.macros, key=lambda x: x.name.lower()):
                f.write(f"- [{macro.name}](macros.md#{macro.name.lower()}) - {macro.description[:50]}{'...' if len(macro.description) > 50 else ''}\n")


def main():
    """Main entry point"""
    if len(sys.argv) < 2:
        print("Usage: python generate_api_docs.py <output_dir> [source_dirs...]")
        print("Example: python generate_api_docs.py docs/api src/common src/client src/server")
        sys.exit(1)

    output_dir = sys.argv[1]
    source_dirs = sys.argv[2:] if len(sys.argv) > 2 else ['src']

    print(f"Generating API documentation to {output_dir}")
    print(f"Processing source directories: {source_dirs}")

    generator = APIDocumentationGenerator(source_dirs)
    generator.process_sources()

    print(f"Found {len(generator.functions)} functions, {len(generator.structs)} structs, {len(generator.macros)} macros")

    generator.generate_markdown_docs(output_dir)
    generator.generate_index(output_dir)

    print("API documentation generation complete!")


if __name__ == '__main__':
    main()
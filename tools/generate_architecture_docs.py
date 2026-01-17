#!/usr/bin/env python3
"""
Architecture Documentation Generator for id Tech 3

This script analyzes the codebase structure and generates system architecture
documentation with interaction diagrams and dependency graphs.
"""

import os
import re
import sys
from typing import Dict, List, Set, Tuple, Optional
from collections import defaultdict
from pathlib import Path
import graphviz

class ArchitectureAnalyzer:
    """Analyzes codebase architecture and dependencies"""

    def __init__(self, source_dirs: List[str]):
        self.source_dirs = source_dirs
        self.modules = {
            'common': {'name': 'Common', 'desc': 'Shared utilities and core systems'},
            'client': {'name': 'Client', 'desc': 'Client-side game logic and rendering'},
            'server': {'name': 'Server', 'desc': 'Server-side game logic and networking'},
            'renderer': {'name': 'Renderer', 'desc': 'Graphics rendering engine'},
            'sound': {'name': 'Audio', 'desc': 'Audio processing and playback'},
            'game': {'name': 'Game', 'desc': 'Server-side game modules'},
            'cgame': {'name': 'Client Game', 'desc': 'Client-side game modules'},
            'ui': {'name': 'UI', 'desc': 'User interface system'}
        }

        self.dependencies: Dict[str, Set[str]] = defaultdict(set)
        self.functions: Dict[str, List[str]] = defaultdict(list)
        self.data_flow: Dict[Tuple[str, str], List[str]] = defaultdict(list)

    def analyze_file(self, file_path: str) -> None:
        """Analyze a single source file for dependencies and architecture"""
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()

            # Determine module
            module = self._get_module_from_path(file_path)

            # Extract function definitions
            func_pattern = r'^(?:static\s+|inline\s+|extern\s+)*([^(\s]+)\s*\([^)]*\)\s*{'
            for match in re.finditer(func_pattern, content, re.MULTILINE):
                func_name = match.group(1).strip()
                self.functions[module].append(func_name)

            # Extract includes to find dependencies
            include_pattern = r'#include\s*["<]([^">]+)[">]'
            for match in re.finditer(include_pattern, content):
                include_file = match.group(1)
                dep_module = self._get_module_from_include(include_file)
                if dep_module and dep_module != module:
                    self.dependencies[module].add(dep_module)

            # Extract function calls to understand data flow
            call_pattern = r'\b([A-Za-z_][A-Za-z0-9_]*)\s*\('
            for match in re.finditer(call_pattern, content):
                func_call = match.group(1)
                # This is a simplified analysis - in practice you'd need
                # more sophisticated call graph analysis
                pass

        except Exception as e:
            print(f"Warning: Could not analyze {file_path}: {e}")

    def _get_module_from_path(self, file_path: str) -> str:
        """Determine module from file path"""
        path_parts = Path(file_path).parts
        for part in path_parts:
            if part in self.modules:
                return part
        return 'common'  # Default

    def _get_module_from_include(self, include_file: str) -> Optional[str]:
        """Determine module from include file path"""
        if '/' in include_file:
            first_part = include_file.split('/')[0]
            if first_part in self.modules:
                return first_part
        return None

    def analyze_codebase(self) -> None:
        """Analyze entire codebase"""
        for source_dir in self.source_dirs:
            if not os.path.exists(source_dir):
                continue

            for root, dirs, files in os.walk(source_dir):
                for file in files:
                    if file.endswith(('.c', '.h', '.cpp', '.hpp')):
                        file_path = os.path.join(root, file)
                        self.analyze_file(file_path)

    def generate_architecture_docs(self, output_dir: str) -> None:
        """Generate architecture documentation"""
        os.makedirs(output_dir, exist_ok=True)

        # Generate main architecture overview
        self._generate_overview(output_dir)

        # Generate module documentation
        self._generate_module_docs(output_dir)

        # Generate dependency graphs
        self._generate_dependency_graph(output_dir)

        # Generate data flow diagrams
        self._generate_data_flow_diagram(output_dir)

    def _generate_overview(self, output_dir: str) -> None:
        """Generate architecture overview"""
        overview_path = os.path.join(output_dir, 'architecture.md')

        with open(overview_path, 'w') as f:
            f.write("# id Tech 3 Architecture Overview\n\n")
            f.write("## System Architecture\n\n")
            f.write("id Tech 3 is a modular game engine with clear separation between client and server components.\n\n")

            f.write("### Core Modules\n\n")
            f.write("| Module | Description | Key Responsibilities |\n")
            f.write("|--------|-------------|---------------------|\n")

            for mod, info in self.modules.items():
                func_count = len(self.functions.get(mod, []))
                dep_count = len(self.dependencies.get(mod, []))
                f.write(f"| {info['name']} | {info['desc']} | {func_count} functions, {dep_count} dependencies |\n")

            f.write("\n### Architecture Principles\n\n")
            f.write("- **Modular Design**: Clear separation of concerns between modules\n")
            f.write("- **Client-Server Model**: Dedicated client and server architectures\n")
            f.write("- **VM-Based Scripting**: Secure execution environment for game logic\n")
            f.write("- **Renderer Abstraction**: Support for multiple graphics APIs\n")
            f.write("- **Plugin Architecture**: Extensible audio and UI systems\n\n")

            f.write("### Data Flow\n\n")
            f.write("1. **Input Processing**: Client captures user input\n")
            f.write("2. **Command Processing**: Commands sent to server via reliable/unreliable channels\n")
            f.write("3. **Game Logic**: Server runs authoritative game simulation\n")
            f.write("4. **State Synchronization**: Game state broadcast to clients\n")
            f.write("5. **Rendering**: Client renders current game state\n")
            f.write("6. **Audio**: Spatial audio mixed and played back\n\n")

            f.write("### Key Technologies\n\n")
            f.write("- **Network Protocol**: Custom reliable UDP-based protocol\n")
            f.write("- **Virtual Machine**: Custom bytecode interpreter for mods\n")
            f.write("- **Rendering Pipeline**: Hardware-accelerated 3D graphics\n")
            f.write("- **Asset Management**: Hierarchical resource loading system\n")
            f.write("- **Memory Management**: Custom allocators and zone-based memory\n\n")

    def _generate_module_docs(self, output_dir: str) -> None:
        """Generate detailed module documentation"""
        for mod, info in self.modules.items():
            module_path = os.path.join(output_dir, f'module_{mod}.md')

            with open(module_path, 'w') as f:
                f.write(f"# {info['name']} Module\n\n")
                f.write(f"{info['desc']}\n\n")

                # Dependencies
                deps = self.dependencies.get(mod, set())
                if deps:
                    f.write("## Dependencies\n\n")
                    f.write("This module depends on:\n\n")
                    for dep in sorted(deps):
                        dep_info = self.modules.get(dep, {'name': dep, 'desc': 'Unknown'})
                        f.write(f"- **{dep_info['name']}**: {dep_info['desc']}\n")
                    f.write("\n")

                # Functions
                funcs = self.functions.get(mod, [])
                if funcs:
                    f.write("## Key Functions\n\n")
                    f.write(f"This module contains {len(funcs)} documented functions:\n\n")

                    # Group functions by category (simplified)
                    api_funcs = [f for f in funcs if not f.startswith('_')]
                    internal_funcs = [f for f in funcs if f.startswith('_')]

                    if api_funcs:
                        f.write("### Public API\n\n")
                        for func in sorted(api_funcs)[:20]:  # Limit for readability
                            f.write(f"- `{func}`\n")
                        if len(api_funcs) > 20:
                            f.write(f"- ... and {len(api_funcs) - 20} more\n")
                        f.write("\n")

                    if internal_funcs:
                        f.write("### Internal Functions\n\n")
                        f.write(f"{len(internal_funcs)} internal implementation functions\n\n")

                # Architecture notes
                self._add_module_specific_notes(f, mod)

    def _add_module_specific_notes(self, f, module: str) -> None:
        """Add module-specific architectural notes"""
        f.write("## Architecture Notes\n\n")

        if module == 'common':
            f.write("The common module provides the foundation for all other modules:\n\n")
            f.write("- **q_shared.h**: Core data types and shared utilities\n")
            f.write("- **qcommon.h**: Common engine functionality\n")
            f.write("- **Memory Management**: Zone and hunk allocators\n")
            f.write("- **File System**: Virtual file system with pak support\n")
            f.write("- **CVAR System**: Configuration variable management\n\n")

        elif module == 'client':
            f.write("The client module handles all user-facing functionality:\n\n")
            f.write("- **Input Processing**: Keyboard, mouse, and gamepad input\n")
            f.write("- **Network Communication**: Client-side networking\n")
            f.write("- **UI System**: Menu and HUD rendering\n")
            f.write("- **Demo Playback**: Recorded game playback\n")
            f.write("- **Screenshot/Cinematic**: Media capture functionality\n\n")

        elif module == 'server':
            f.write("The server module manages authoritative game state:\n\n")
            f.write("- **Game Simulation**: Authoritative physics and game logic\n")
            f.write("- **Client Management**: Player connections and state\n")
            f.write("- **Network Broadcasting**: Game state synchronization\n")
            f.write("- **Anti-Cheat**: Server-side validation and security\n")
            f.write("- **Rate Limiting**: DDoS protection and fair play\n\n")

        elif module == 'renderer':
            f.write("The renderer module provides hardware-accelerated graphics:\n\n")
            f.write("- **Shader Management**: GLSL and HLSL shader compilation\n")
            f.write("- **Texture Processing**: Image loading and compression\n")
            f.write("- **Geometry Pipeline**: Vertex processing and batching\n")
            f.write("- **Lighting**: Dynamic and static lighting systems\n")
            f.write("- **Post-Processing**: Effects and image filters\n\n")

        f.write("## Performance Characteristics\n\n")
        f.write("*(Performance analysis would be added based on profiling data)*\n\n")

        f.write("## Security Considerations\n\n")
        f.write("*(Security analysis and hardening notes would be documented here)*\n\n")

    def _generate_dependency_graph(self, output_dir: str) -> None:
        """Generate dependency graph visualization"""
        dot = graphviz.Digraph('architecture_dependencies', comment='id Tech 3 Module Dependencies')

        # Add nodes
        for mod, info in self.modules.items():
            func_count = len(self.functions.get(mod, []))
            dot.node(mod, f"{info['name']}\\n({func_count} functions)", shape='box')

        # Add edges
        for mod, deps in self.dependencies.items():
            for dep in deps:
                if dep in self.modules:  # Only show known modules
                    dot.edge(mod, dep)

        # Save graph
        graph_path = os.path.join(output_dir, 'dependency_graph')
        dot.render(graph_path, format='png', cleanup=True)

        # Generate text description
        dep_path = os.path.join(output_dir, 'dependencies.md')
        with open(dep_path, 'w') as f:
            f.write("# Module Dependencies\n\n")
            f.write("## Dependency Graph\n\n")
            f.write("![Module Dependencies](dependency_graph.png)\n\n")
            f.write("## Detailed Dependencies\n\n")

            for mod, info in self.modules.items():
                deps = self.dependencies.get(mod, set())
                if deps:
                    f.write(f"### {info['name']} Depends On:\n\n")
                    for dep in sorted(deps):
                        dep_info = self.modules.get(dep, {'name': dep})
                        f.write(f"- {dep_info['name']}\n")
                    f.write("\n")

    def _generate_data_flow_diagram(self, output_dir: str) -> None:
        """Generate data flow diagram"""
        flow_path = os.path.join(output_dir, 'data_flow.md')

        with open(flow_path, 'w') as f:
            f.write("# Data Flow Architecture\n\n")
            f.write("## High-Level Data Flow\n\n")
            f.write("```\n")
            f.write("User Input → Client Processing → Network Transmission → Server Processing → State Update\n")
            f.write("                                                                             ↓\n")
            f.write("                                                             Network Broadcast ← Game State\n")
            f.write("                                                                             ↓\n")
            f.write("                                                    Client Reception → State Sync → Rendering → Display\n")
            f.write("```\n\n")

            f.write("## Detailed Component Interactions\n\n")
            f.write("### Client-Server Communication\n\n")
            f.write("1. **Connection Establishment**\n")
            f.write("   - Client sends connection request\n")
            f.write("   - Server validates and accepts connection\n")
            f.write("   - Initial game state synchronization\n\n")

            f.write("2. **Gameplay Communication**\n")
            f.write("   - Client sends user commands (movement, actions)\n")
            f.write("   - Server processes commands and updates authoritative state\n")
            f.write("   - Server broadcasts state updates to all clients\n")
            f.write("   - Clients interpolate and predict state changes\n\n")

            f.write("3. **Asset Loading**\n")
            f.write("   - Client requests game assets from server\n")
            f.write("   - Server validates and serves approved assets\n")
            f.write("   - Client caches assets for performance\n\n")

            f.write("### Internal Data Flow\n\n")
            f.write("#### Client Data Flow\n")
            f.write("- Input → Command Buffer → Network → Prediction → Rendering\n\n")

            f.write("#### Server Data Flow\n")
            f.write("- Network → Command Processing → Physics → AI → State Broadcast\n\n")

            f.write("#### Renderer Data Flow\n")
            f.write("- Game State → Culling → Sorting → Batching → GPU Submission\n\n")


def main():
    """Main entry point"""
    if len(sys.argv) < 2:
        print("Usage: python generate_architecture_docs.py <output_dir> [source_dirs...]")
        print("Example: python generate_architecture_docs.py docs/architecture src")
        sys.exit(1)

    output_dir = sys.argv[1]
    source_dirs = sys.argv[2:] if len(sys.argv) > 2 else ['src']

    print(f"Generating architecture documentation to {output_dir}")
    print(f"Analyzing source directories: {source_dirs}")

    analyzer = ArchitectureAnalyzer(source_dirs)
    analyzer.analyze_codebase()

    print(f"Found {sum(len(funcs) for funcs in analyzer.functions.values())} functions")
    print(f"Found {len(analyzer.dependencies)} dependency relationships")

    analyzer.generate_architecture_docs(output_dir)

    print("Architecture documentation generation complete!")


if __name__ == '__main__':
    main()
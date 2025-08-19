#!/usr/bin/env python3

import configparser
import shlex
from pathlib import Path
from util import log

class BoardConfig:
    """Board configuration parser with support for tagged sections"""
    
    def __init__(self, board_name):
        self.board_name = board_name
        self.config_file = Path(f"environment/{board_name}/board.cfg")
        self.config = configparser.ConfigParser()
        self.load_config()
    
    def load_config(self):
        """Load configuration from board-specific file"""
        if not self.config_file.exists():
            log("ERROR", f"Board config not found: {self.config_file}")
        
        self.config.read(self.config_file)
    
    def _parse_list(self, value):
        """Parse list values from config (space-separated format)"""
        if not value:
            return []
        
        value = value.strip()
        if not value:
            return []
        
        try:
            # Use shlex.split() to handle quoted arguments properly
            return shlex.split(value)
        except ValueError as e:
            # Fallback to simple split if shlex fails (e.g., unmatched quotes)
            log("WARNING", f"Failed to parse config value '{value}' with shlex: {e}")
            return value.split()
    
    def _get_section_key(self, section, key, default=None):
        """Get key from section with fallback to default"""
        if section in self.config and key in self.config[section]:
            value = self.config[section][key]
            if key.endswith('_cflags') or key.endswith('_ldflags') or key.endswith('_includes') or \
               key.endswith('_defines') or key.endswith('_sources') or key in ['libs', 'includes', 'defines', 'base_cflags', 'base_ldflags', 'environment_files', 'skip_common_files', 'workload_sources']:
                return self._parse_list(value)
            return value
        return default
    
    def get_build_config(self, arch, platform, workload=None, bbv=False, trace=False, 
                        benchmark_name=None):
        """Get complete build configuration for given parameters"""
        config = {}
        
        # Start with DEFAULT section
        default_section = 'DEFAULT'
        if default_section in self.config:
            for key in self.config[default_section]:
                config[key] = self._get_section_key(default_section, key)
        
        # Apply architecture + platform specific settings
        arch_platform = f"{arch}.{platform}"
        if arch_platform in self.config:
            for key in self.config[arch_platform]:
                config[key] = self._get_section_key(arch_platform, key)
        
        # Apply workload-specific settings
        if workload and workload in self.config:
            for key in self.config[workload]:
                value = self._get_section_key(workload, key)
                if key.startswith('workload_'):
                    # Special handling for workload_sources - preserve full key name
                    if key == 'workload_sources':
                        config[key] = value
                    else:
                        # Merge workload-specific flags
                        base_key = key.replace('workload_', '')
                        if base_key in config:
                            if isinstance(config[base_key], list):
                                config[base_key].extend(value or [])
                            else:
                                config[base_key] = value
                        else:
                            config[base_key] = value
                else:
                    config[key] = value
        
        # Apply platform + workload specific overrides
        platform_workload = f"{platform}.{workload}" if workload else None
        if platform_workload and platform_workload in self.config:
            for key in self.config[platform_workload]:
                config[key] = self._get_section_key(platform_workload, key)
        
        # Apply BBV settings if enabled
        if bbv and 'bbv' in self.config:
            bbv_cflags = self._get_section_key('bbv', 'bbv_cflags', [])
            bbv_ldflags = self._get_section_key('bbv', 'bbv_ldflags', [])
            
            config.setdefault('base_cflags', []).extend(bbv_cflags)
            config.setdefault('base_ldflags', []).extend(bbv_ldflags)
        
        # Apply trace settings if enabled
        if trace and 'trace' in self.config:
            trace_cflags = self._get_section_key('trace', 'trace_cflags', [])
            trace_ldflags = self._get_section_key('trace', 'trace_ldflags', [])
            
            config.setdefault('base_cflags', []).extend(trace_cflags)
            config.setdefault('base_ldflags', []).extend(trace_ldflags)
        
        # Handle vector benchmarks (override architecture) - only for riscv-tests workload
        if workload == "riscv-tests" and 'vector' in self.config:
            vector_config = self.config['vector']
            if benchmark_name and benchmark_name.startswith('vec-'):
                # Vector benchmarks get vector architecture
                arch_key = f"vector_{arch}_arch"
                if arch_key in vector_config:
                    new_arch = vector_config[arch_key]
                    # Update march flags
                    if 'base_cflags' in config:
                        for i, flag in enumerate(config['base_cflags']):
                            if flag.startswith('-march='):
                                config['base_cflags'][i] = f"-march={new_arch}"
                    if 'base_ldflags' in config:
                        for i, flag in enumerate(config['base_ldflags']):
                            if flag.startswith('-march='):
                                config['base_ldflags'][i] = f"-march={new_arch}"
            else:
                # Non-vector riscv-tests benchmarks get regular arch  
                arch_key = f"regular_{arch}_arch"
                if arch_key in vector_config:
                    new_arch = vector_config[arch_key]
                    # Update march flags
                    if 'base_cflags' in config:
                        for i, flag in enumerate(config['base_cflags']):
                            if flag.startswith('-march='):
                                config['base_cflags'][i] = f"-march={new_arch}"
                    if 'base_ldflags' in config:
                        for i, flag in enumerate(config['base_ldflags']):
                            if flag.startswith('-march='):
                                config['base_ldflags'][i] = f"-march={new_arch}"
        # Note: embench-iot and other workloads preserve their base architecture from [rv32.baremetal]
        
        return config
    
    def get_compiler_info(self, arch, platform):
        """Get compiler and base flags for architecture and platform"""
        config = self.get_build_config(arch, platform)
        
        cc = config.get('cc', f"riscv{arch[2:]}-unknown-elf-gcc")
        base_cflags = ' '.join(config.get('base_cflags', []))
        base_ldflags = ' '.join(config.get('base_ldflags', []))
        
        return cc, base_cflags, base_ldflags
    
    def get_environment_files(self, workload=None):
        """Get list of environment files to compile"""
        config = self.get_build_config("rv32", "baremetal", workload)
        return config.get('environment_files', ['crt0.S', 'main.c', 'stub.c'])
    
    def should_skip_environment(self, platform, workload=None):
        """Check if environment compilation should be skipped"""
        config = self.get_build_config("rv32", platform, workload)
        return config.get('skip_environment', False)
    
    def get_workload_includes(self, workload_path, workload=None):
        """Get workload-specific include paths"""
        config = self.get_build_config("rv32", "baremetal", workload)
        includes = config.get('includes', [])
        
        # Convert relative paths to absolute based on workload_path
        absolute_includes = []
        for include in includes:
            if not Path(include).is_absolute():
                absolute_includes.append(str(Path(workload_path) / include))
            else:
                absolute_includes.append(include)
        
        return absolute_includes
    
    def get_skip_common_files(self, platform, workload=None):
        """Get list of common files to skip for this platform/workload"""
        config = self.get_build_config("rv32", platform, workload)
        return config.get('skip_common_files', [])
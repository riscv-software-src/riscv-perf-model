#!/usr/bin/env python3

import yaml
import shlex
from pathlib import Path
from utils.util import log

class BoardConfig:
    """Board configuration parser with support for tagged sections"""
    
    def __init__(self, board_name):
        self.board_name = board_name
        self.config_file = Path(f"environment/{board_name}/board.yaml")
        self.config = {}
        self.load_config()
    
    def load_config(self):
        """Load configuration from board-specific file"""
        if not self.config_file.exists():
            log("ERROR", f"Board config not found: {self.config_file}")
            return
        
        with open(self.config_file, 'r') as f:
            self.config = yaml.safe_load(f) or {}
    
    def _parse_list(self, value):
        """Parse list values from config (YAML native lists or space-separated strings)"""
        if not value:
            return []
        
        # If already a list, return as-is
        if isinstance(value, list):
            return value
        
        # If string, parse as space-separated (backward compatibility)
        if isinstance(value, str):
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
        
        return []
    
    def _get_nested_value(self, path, default=None):
        """Get value from nested YAML structure using dot notation"""
        current = self.config
        try:
            for key in path.split('.'):
                current = current[key]
            
            # Parse lists for known list-type keys
            if isinstance(path.split('.')[-1], str):
                key = path.split('.')[-1]
                if (key.endswith('_cflags') or key.endswith('_ldflags') or key.endswith('_includes') or 
                    key.endswith('_defines') or key.endswith('_sources') or 
                    key in ['libs', 'includes', 'defines', 'base_cflags', 'base_ldflags', 
                           'environment_files', 'skip_common_files', 'workload_sources']):
                    return self._parse_list(current)
            
            return current
        except (KeyError, TypeError):
            return default
    
    def _get_legacy_section_key(self, section, key, default=None):
        """Legacy method for backward compatibility - maps to new structure"""
        # Handle legacy section names
        if section == 'DEFAULT':
            return self._get_nested_value(f'defaults.{key}', default)
        
        # Handle architecture.platform sections (e.g., rv32.baremetal)
        if '.' in section and len(section.split('.')) == 2:
            arch, platform = section.split('.')
            return self._get_nested_value(f'architectures.{arch}.platforms.{platform}.{key}', default)
        
        # Handle workload sections
        if section in ['embench-iot', 'riscv-tests', 'dhrystone']:
            return self._get_nested_value(f'workloads.{section}.{key}', default)
        
        # Handle platform.workload sections (e.g., linux.embench-iot)
        if '.' in section and section.split('.')[0] in ['linux', 'baremetal']:
            platform, workload = section.split('.')
            return self._get_nested_value(f'workloads.{workload}.platforms.{platform}.{key}', default)
        
        # Handle special sections
        if section in ['bbv', 'trace', 'vector']:
            return self._get_nested_value(f'features.{section}.{key}', default)
        
        return default
    
    def get_build_config(self, arch, platform, workload=None, bbv=False, trace=False, 
                        benchmark_name=None):
        """Get complete build configuration for given parameters"""
        config = {}
        
        # Start with defaults
        defaults = self._get_nested_value('defaults', {})
        for key, value in defaults.items():
            if (key.endswith('_cflags') or key.endswith('_ldflags') or key.endswith('_includes') or 
                key.endswith('_defines') or key.endswith('_sources') or 
                key in ['libs', 'includes', 'defines', 'base_cflags', 'base_ldflags', 
                       'environment_files', 'skip_common_files', 'workload_sources']):
                config[key] = self._parse_list(value)
            else:
                config[key] = value
        
        # Apply architecture + platform specific settings
        arch_platform_config = self._get_nested_value(f'architectures.{arch}.platforms.{platform}', {})
        for key, value in arch_platform_config.items():
            if (key.endswith('_cflags') or key.endswith('_ldflags') or key.endswith('_includes') or 
                key.endswith('_defines') or key.endswith('_sources') or 
                key in ['libs', 'includes', 'defines', 'base_cflags', 'base_ldflags', 
                       'environment_files', 'skip_common_files', 'workload_sources']):
                config[key] = self._parse_list(value)
            else:
                config[key] = value
        
        # Apply workload-specific settings
        if workload:
            workload_config = self._get_nested_value(f'workloads.{workload}', {})
            for key, value in workload_config.items():
                if key.startswith('workload_'):
                    # Special handling for workload_sources - preserve full key name
                    if key == 'workload_sources':
                        config[key] = self._parse_list(value)
                    else:
                        # Merge workload-specific flags
                        base_key = key.replace('workload_', '')
                        parsed_value = self._parse_list(value) if (
                            key.endswith('_cflags') or key.endswith('_ldflags') or key.endswith('_includes') or 
                            key.endswith('_defines') or key.endswith('_sources')
                        ) else value
                        
                        if base_key in config:
                            if isinstance(config[base_key], list) and isinstance(parsed_value, list):
                                config[base_key].extend(parsed_value)
                            else:
                                config[base_key] = parsed_value
                        else:
                            config[base_key] = parsed_value
                else:
                    if (key.endswith('_cflags') or key.endswith('_ldflags') or key.endswith('_includes') or 
                        key.endswith('_defines') or key.endswith('_sources') or 
                        key in ['libs', 'includes', 'defines', 'base_cflags', 'base_ldflags', 
                               'environment_files', 'skip_common_files', 'workload_sources']):
                        config[key] = self._parse_list(value)
                    else:
                        config[key] = value
            
            # Apply platform-specific workload overrides
            platform_workload_config = self._get_nested_value(f'workloads.{workload}.platforms.{platform}', {})
            for key, value in platform_workload_config.items():
                if (key.endswith('_cflags') or key.endswith('_ldflags') or key.endswith('_includes') or 
                    key.endswith('_defines') or key.endswith('_sources') or 
                    key in ['libs', 'includes', 'defines', 'base_cflags', 'base_ldflags', 
                           'environment_files', 'skip_common_files', 'workload_sources']):
                    config[key] = self._parse_list(value)
                else:
                    config[key] = value
        
        # Apply BBV settings if enabled
        if bbv:
            bbv_cflags = self._get_nested_value('features.bbv.bbv_cflags', [])
            bbv_ldflags = self._get_nested_value('features.bbv.bbv_ldflags', [])
            
            config.setdefault('base_cflags', []).extend(self._parse_list(bbv_cflags))
            config.setdefault('base_ldflags', []).extend(self._parse_list(bbv_ldflags))
        
        # Apply trace settings if enabled
        if trace:
            trace_cflags = self._get_nested_value('features.trace.trace_cflags', [])
            trace_ldflags = self._get_nested_value('features.trace.trace_ldflags', [])
            
            config.setdefault('base_cflags', []).extend(self._parse_list(trace_cflags))
            config.setdefault('base_ldflags', []).extend(self._parse_list(trace_ldflags))
        
        # Handle vector benchmarks (override architecture) - only for riscv-tests workload
        if workload == "riscv-tests":
            vector_config = self._get_nested_value('features.vector', {})
            if vector_config:
                if benchmark_name and benchmark_name.startswith('vec-'):
                    # Vector benchmarks get vector architecture
                    arch_key = f"vector_{arch}_arch"
                    new_arch = vector_config.get(arch_key)
                    if new_arch:
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
                    new_arch = vector_config.get(arch_key)
                    if new_arch:
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
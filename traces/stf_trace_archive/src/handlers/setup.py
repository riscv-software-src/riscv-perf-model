import argparse
import os
import pathlib
import yaml
from dataclasses import asdict
from data.config import Config, StorageConfig
from data.source_type_map import SOURCE_TYPE_MAP
from .base import CommandHandler

class SetupHandler(CommandHandler):
    def __init__(self):
        config_folder = pathlib.Path(__file__).parent.parent.resolve()
        config_filename = "config.yaml"
        self._config_path = os.path.join(config_folder, config_filename)
        self._config: Config = None
        self._read_config_file()
        self._complete_config_file()

    def run(self, args: argparse.Namespace, _) -> None:
        if args.add_storage:
            self._add_storage_source()
            self._save_config()

        elif args.set_default_storage:
            self._set_default_storage(args.set_default_storage)
            self._save_config()

    def get_config(self) -> Config:
        return self._config

    def _read_config_file(self) -> None:
        if not os.path.exists(self._config_path):
            return

        with open(self._config_path, 'r') as config_file:
            config_dict = yaml.safe_load(config_file)
            self._config = Config.from_dict(config_dict)
            print("config")
            print(self._config)

    def _complete_config_file(self) -> None:
        if not self._config or not self._config.storages:
            print("Config is empty or invalid. Creating new config file")
            self._add_storage_source()
            self._save_config()

        if not self._config.default_storage:
            print("Default storage not set.")
            self._set_default_storage()
            self._save_config()


    def _add_storage_source(self) -> None:
        source_types = list(SOURCE_TYPE_MAP.keys())
        print("Creating a new storage source.")
        print(f"Registred source type options: {', '.join(source_types)}")
        source_type = input("Select your source type: ").lower()

        if source_type not in source_types:
            raise ValueError(f"Unknown source type: {source_type}")

        used_source_names = self._get_source_names()
        source_name = input("Enter your source name: ").lower()

        if source_name in used_source_names:
            raise ValueError(f"Source name {source_name} already in use")

        source_class = SOURCE_TYPE_MAP.get(source_type)
        source_specific_config = source_class.setup()
        source_config = StorageConfig(type = source_type, name=source_name, config=source_specific_config)

        if not self._config:
            self._config = Config(storages=[source_config], default_storage=source_name)
        elif not self._config.storages:
            self._config.storages = [source_config]
        else:
            self._config.storages.append(source_config)

        if not self._config.default_storage:
            self._config.default_storage = source_name

    def _get_source_names(self) -> list[str]:
        if not self._config:
            return []
        
        return [storage.name for storage in self._config.storages]
    
    def _set_default_storage(self, source_name: str = None) -> None:
        used_source_names = self._get_source_names()

        if not source_name:
            print(f"Enter the default source name: ", end="")
            source_name = input().lower()

        if source_name not in used_source_names:
            raise ValueError(f"Source name {source_name} not found on configure storage source names")

        self._config.default_storage = source_name

    def _save_config(self) -> None:
        with open(self._config_path, 'w') as config_file:
            yaml.safe_dump(asdict(self._config), config_file)
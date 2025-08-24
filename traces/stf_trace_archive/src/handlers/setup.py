import argparse
import os
import pathlib
import yaml
from dataclasses import asdict
from data.config import Config, StorageConfig
from data.storage_type_map import STORAGE_TYPE_MAP
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
        storage_types = list(STORAGE_TYPE_MAP.keys())
        print("Creating a new storage source.")
        print(f"Registred storage type options: {', '.join(storage_types)}")
        storage_type = input("Select your storage type: ").lower()

        if storage_type not in storage_types:
            raise ValueError(f"Unknown storage type: {storage_type}")

        used_storage_names = self._get_storage_names()
        storage_name = input("Enter your storage name: ").lower()

        if storage_name in used_storage_names:
            raise ValueError(f"Storage name {storage_name} already in use")

        storage_class = STORAGE_TYPE_MAP.get(storage_type)
        storage_specific_config = storage_class.setup()
        storage_config = StorageConfig(
            type=storage_type, name=storage_name, config=storage_specific_config)

        if not self._config:
            self._config = Config(
                storages=[storage_config], default_storage=storage_name)
        elif not self._config.storages:
            self._config.storages = [storage_config]
        else:
            self._config.storages.append(storage_config)

        if not self._config.default_storage:
            self._config.default_storage = storage_name

    def _get_storage_names(self) -> list[str]:
        if not self._config:
            return []

        return [storage.name for storage in self._config.storages]

    def _set_default_storage(self, storage_name: str = None) -> None:
        used_storage_names = self._get_storage_names()

        if not storage_name:
            storage_name = input("Enter the default storage source name: ").lower()

        if storage_name not in used_storage_names:
            raise ValueError(f"Storage source name {storage_name} not found on configured names")

        self._config.default_storage = storage_name

    def _save_config(self) -> None:
        with open(self._config_path, 'w') as config_file:
            yaml.safe_dump(asdict(self._config), config_file)

from dataclasses import dataclass
from typing import Dict, Optional, Type, Union, List


@dataclass
class LocalStorageConfig:
    path: str


CONFIG_TYPE_MAP: Dict[str, Type] = {
    "local-storage": LocalStorageConfig,
}


@dataclass
class StorageConfig:
    type: str
    name: str
    config: Union[LocalStorageConfig]

    @staticmethod
    def from_dict(data: dict):
        specific_config_type = data['type']
        if specific_config_type not in CONFIG_TYPE_MAP:
            raise ValueError(f"Unknown storage type: {specific_config_type}")

        specific_config_class = CONFIG_TYPE_MAP.get(specific_config_type)
        specific_config = specific_config_class(**data['config'])
        return StorageConfig(type=data['type'], name=data['name'], config=specific_config)


@dataclass
class Config:
    storages: List[StorageConfig]
    default_storage: Optional[str]

    @staticmethod
    def from_dict(data: dict):
        if not data:
            return None

        storages = []
        if 'storages' in data:
            storages = [StorageConfig.from_dict(s) for s in data['storages']]

        return Config(storages=storages, default_storage=data.get('default_storage'))

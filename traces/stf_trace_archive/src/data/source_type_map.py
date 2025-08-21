from typing import Type, Dict
from database_explorer.sources.local_storage import LocalStorageSource

SOURCE_TYPE_MAP: Dict[str, Type] = {
    "local-storage": LocalStorageSource,
}
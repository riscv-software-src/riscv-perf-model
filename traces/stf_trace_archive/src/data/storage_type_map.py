from typing import Type, Dict
from data.storages.local_storage import LocalStorage

STORAGE_TYPE_MAP: Dict[str, Type] = {
    "local-storage": LocalStorage,
}

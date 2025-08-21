from data.config import Config, StorageConfig
from data.source_type_map import SOURCE_TYPE_MAP
from database_explorer.database_explorer import DatabaseExplorer
from utils.cli_parser import parseArgs
from handlers.upload import UploadHandler
from handlers.list import ListHandler
from handlers.search import SearchHandler
from handlers.get import GetHandler
from handlers.setup import SetupHandler

def main():
    args = parseArgs()
    setupHandler = SetupHandler()
    command_map = {
        'upload': UploadHandler(),
        'search': SearchHandler(),
        'list': ListHandler(),
        'get': GetHandler(),
        'setup': setupHandler,
    }

    config: Config = setupHandler.get_config()
    explorer = get_storage(args.storage_name, config)

    handler = command_map.get(args.command)
    if handler:
        handler.run(args, explorer)
    else:
        print(f"Unknown command: {args.command}")

def get_storage(selected_storage: str, config: Config) -> DatabaseExplorer:
    if not selected_storage:
        selected_storage = config.default_storage

    storage_config = None
    for storage in config.storages:
        if storage.name == selected_storage:
            storage_config = storage
            break

    if not storage_config:
        raise ValueError(f"Storage not found: {selected_storage}")
    
    storage_class = SOURCE_TYPE_MAP.get(storage_config.type)
    if not storage_class:
        raise ValueError(f"Unknown source storage class: {storage_class}")
    
    storage = storage_class(storage_config.config)
    explorer = DatabaseExplorer(storage)
    return explorer

if __name__ == "__main__":
    main()
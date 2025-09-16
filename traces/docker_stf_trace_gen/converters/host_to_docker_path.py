import os
from converters.base import BaseConverter
from data.consts import Const


class HostToDockerPathConverter(BaseConverter):
    @staticmethod
    def convert(path: str) -> str:
        parts = os.path.abspath(path).strip(os.sep).split(os.sep)
        parts.insert(0, Const.DOCKER_TEMP_FOLDER)
        return os.path.join(*parts)

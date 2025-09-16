from abc import ABC
from typing import Any


class BaseConverter(ABC):
    @staticmethod
    def convert(self, input: Any) -> Any:
        raise NotImplementedError("This method should be overridden by subclasses.")

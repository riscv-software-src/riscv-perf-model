from dataclasses import dataclass


@dataclass(frozen=True)
class WorkloadsTableSchema:
    WORKLOAD_ID: str = "workload_id"
    WORKLOAD_NAME: str = "workload_name"

    def get_columns() -> list[str]:
        return [
            WorkloadsTableSchema.WORKLOAD_ID,
            WorkloadsTableSchema.WORKLOAD_NAME,
        ]

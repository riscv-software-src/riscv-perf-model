from dataclasses import dataclass


@dataclass(frozen=True)
class TracesTableSchema:
    TRACE_ID: str = "trace_id"
    TRACE_ATTEMPT: str = "trace_attempt"
    TRACE_PART: str = "trace_part"
    WORKLOAD_ID: str = "workload_id"
    WORKLOAD_SHA: str = "workload_sha"
    WORKLOAD_NAME: str = "workload_name"
    FULLY_TRACED: str = "fully_traced"

    def get_columns() -> list[str]:
        return [
            TracesTableSchema.TRACE_ID,
            TracesTableSchema.TRACE_ATTEMPT,
            TracesTableSchema.TRACE_PART,
            TracesTableSchema.WORKLOAD_ID,
            TracesTableSchema.WORKLOAD_SHA,
            TracesTableSchema.WORKLOAD_NAME,
            TracesTableSchema.FULLY_TRACED
        ]

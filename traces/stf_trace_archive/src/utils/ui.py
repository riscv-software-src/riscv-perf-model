from data.metadata import Metadata

def print_medatata_details(metadata: Metadata) -> None:
    print(f"id: {metadata['trace_id']}")
    if "description" in metadata and metadata['description']:
        print(metadata['description'])

    print(f"Workload: {metadata['workload']['filename']}")
    print(f"Trace Timestamp: {metadata['stf']['timestamp']}")
    print_metadata_interval(metadata)

    print("\n---------------------------------")

def print_metadata_interval(metadata: Metadata) -> None:
    if "trace_interval" not in metadata['stf'] or metadata['stf']["trace_interval"] is None:
        print("Fully trace")
        return
    
    trace_interval = metadata['stf']['trace_interval']
    print(f"Trace Interval:")
    if trace_interval['instruction_pc'] is not None:
        print(f"  Instruction PC: {trace_interval['instruction_pc']}")

    if trace_interval['pc_count'] is not None:
        print(f"  PC Count: {trace_interval['pc_count']}")
    
    if trace_interval['interval_lenght'] is not None:
        print(f"  Interval Length: {trace_interval['interval_lenght']}")
    
    if trace_interval['start_instruction_index'] is not None:
        print(f"  Start Instruction Index: {trace_interval['start_instruction_index']}")
    
    if trace_interval['end_instruction_index'] is not None:
        print(f"  End Instruction Index: {trace_interval['end_instruction_index']}")
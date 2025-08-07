# Trace Archive Tool

A Python command-line interface (CLI) tool to manage shared trace files,such as uploding, searching and downloading traces.

## Usage

Run the script using:

```bash
python trace_archive.py <command> [options]
```

To view all available commands and options use `--help` or `-h`:

```bash
$ python trace_archive.py --help
Usage: python trace_archive.py COMMAND [OPTIONS]

CLI tool for Olympia traces exploration

Commands:
  connect    Connect to the system or database.
  upload     Upload workload and trace.
  search     Search traces by specified expression.
  list       List items by category.
  get        Download a specified trace file.

Run 'trace_archive COMMAND --help' for more information on a command.

For more help on how to use trace_archive, head to GITHUB_README_LINK
```

---

## Available Commands

### `upload`

Uploads a trace file along with its associated workload and metadata.

```bash
$ python trace_archive.py upload --help
Usage:  python trace_archive.py upload [OPTIONS]

Upload a workload, trace and metadata to the database

Options:
      --workload  Path to the workload file.
      --trace     Path to the trace file.
      --it        Iteractive files selection mode.
```

> Requires a metadata file located at `<trace>.metadata.yaml`.

> For every upload, a unqiue [trace id](#trace-id) will be generated

---

### `search`

Search can be used to search for the given regex term in the list of available traces and metadata matches

```bash
$ python trace_archive.py search --help
Usage:  python trace_archive.py search [OPTIONS] [REGEX]

Search for traces and metadata using a regular expression.

Arguments:
      REGEX             Regex expression to search with.

Options:
      --names-only      Search only by trace id (ignore metadata).
```

---

### `list`

```bash
$ python trace_archive.py list --help
Usage:  python trace_archive.py list [OPTIONS]

List database traces or related entities.

Options:
      --traces        Lists available traces (default)
      --workloads     Lists available workloads
```

---

### `get`

Downloads a specified trace file.

```bash
$ python trace_archive.py get --help
Usage:  python trace_archive.py get [OPTIONS] TRACE

Download a specified trace file.

Arguments:
      TRACE             Name of the trace to download.

Options:
      --revision        Revision number. If not specified, the latest revision is used.
      --company         Filter by associated company.
      --author          Filter by author.
      -o, --output      Output file path.
```

## Trace ID

When a trace file is uploaded to the trace archive, a `trace_id` is automatically created and filled into the metadata. The `trace_id` follows the structure:

```text
<workload id>.<trace attempt>.<trace.part>_<workload filename>
```

Where:

- **`workload id`**: A **sequential integer**, uniquely identifying a workload **based on its SHA256**. Assigned in upload order (e.g., `0`, `1`, `2`, ...).
- **`trace attempt`**: A **sequential number** representing a distinct tracing process for the same workload — whether it’s a full trace or a colletion of trace parts.
- **`trace_part`**: A **sequential index** of the specific part within a trace attempt.

  - For a **fully traced workload**, this will always be `0`.
  - For a **partial trace**, each part is numbered in upload order (e.g., `0`, `1`, `2`, ...).

- **`workload filename`**: Taken directly from the `workload.filename` field.

---

### Example Trace IDs

| Upload # | Description                                               | Trace ID                |
| -------- | --------------------------------------------------------- | ----------------------- |
| 1st      | `dhrystone` compiled with `-O3`, fully traced             | `000.000.000_dhrystone` |
| 2nd      | `dhrystone` `-O3`, traced from instruction 0 to 1,000,000 | `000.001.000_dhrystone` |
| 3rd      | `dhrystone` `-O3`, traced from 1,000,000 to 2,000,000     | `000.001.001_dhrystone` |
| 4th      | `dhrystone` `-O3`, traced from 2,000,000 to 3,000,000     | `000.001.002_dhrystone` |
| 5th      | Same trace as 1st (re-uploaded)                           | `000.002.000_dhrystone` |
| 6th      | `dhrystone` compiled with `-O2`, fully traced             | `001.000.000_dhrystone` |
| 7th      | `embench` compiled with `-O3`, fully traced               | `002.000.000_embench`   |

---

## Storage Folder Structure

For the trace archive structure, each workload is stored in its own folder, identified by its workload_id. This folder contains the workload file, related outputs (e.g., stdout, objdump), and subdirectories for each trace attempt.

The tree graph below illustrates a setup of the [Trace Id Example](#example-trace-ids):

```text
000/
├── dhrystone
├── dhrystone.objdump
├── dhrystone.stdout
├── 000/
│   ├── 000.000.000_dhrystone.zstf
│   └── 000.000.000_dhrystone.zstf.metadata.yaml
├── 001/
│   ├── 000.001.000_dhrystone.zstf
│   ├── 000.001.000_dhrystone.zstf.metadata.yaml
│   ├── 000.001.001_dhrystone.zstf
│   ├── 000.001.001_dhrystone.zstf.metadata.yaml
│   ├── 000.001.002_dhrystone.zstf
│   └── 000.001.002_dhrystone.zstf.metadata.yaml
001/
├── dhrystone
├── dhrystone.objdump
├── dhrystone.stdout
└── 000/
    ├── 001.000.000_dhrystone.zstf
    └── 001.000.000_dhrystone.zstf.metadata.yaml
002/
├── embench.zip
├── embench.objdump
├── embench.stdout
└── 000/
    ├── 002.000.000_embench.zstf
    └── 002.000.000_embench.zstf.metadata.yaml

```

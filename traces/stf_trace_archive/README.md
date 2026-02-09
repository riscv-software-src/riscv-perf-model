# Trace Archive Tool

## Table of Contents

1. [Quickstart](#quickstart)
2. [Introduction](#introduction)
3. [Dependencies](#dependencies)
4. [Project Structure](#project-structure)
5. [Usage](#usage)
    1. [Initial Setup](#initial-setup)
    2. [Upload Command](#upload-command)
    3. [List Command](#list-command)
    4. [Get Command](#get-command)
6. [Examples](#examples)
    1. [Uploading a Trace](#uploading-a-trace)
    2. [Downloading a Trace](#downloading-a-trace)
    3. [Downloading a Workload](#downloading-a-workload)
    4. [Creating and using second storage source](#creating-and-using-second-storage-source)
7. [Trace ID](#trace-id)
    1. [Example Trace IDs](#example-trace-ids)
8. [Storage Folder Structure](#storage-folder-structure)

## Quickstart

```bash
# Install dependencies
pip install -r requirements.txt

# Configure initial storage (local)
python trace_archive.py setup

# Upload a workload and trace
python trace_archive.py upload --workload ../../stf_metadata/example/dhrystone --trace ../../stf_metadata/example/dhrystone.zstf
```

## Introduction

A Python CLI tool for uploading, organizing, and sharing trace and workload files.
Currently supports local storage, with planned extensions for cloud sources (e.g., Google Drive).

## Dependencies

To use the trace archive tool, ensure you have the following installed:

- **Python 3** (recommended: Python 3.8 or newer)
- **Required Python packages**: Install dependencies with:
    ```bash
    pip install -r requirements.txt
    ```
    The main requirements are:
    - `pandas`
    - `PyYAML`

## Project Structure

```text
src/
├── data/                  # Core data models and classes
│   └── storage/           # Storage backend implementations (local, cloud, etc.)
├── handlers/              # Command handlers (upload, get, list, setup)
├── utils/                 # Utility functions and helpers
└── trace_archive.py       # Main CLI entry point
```


## Usage

The tool can be used with the following commands:

* **[Upload](#upload-command).** Upload workload and/or trace.
* **[List](#list-command).** List items by category.
* **[Get](#get-command).** Download a specified trace file.
* **[Setup](#setup-command).** Create or edit current tool configurations.


### Initial Setup

To set up the trace archive tool, run the `setup` command to configure the inital storage type and it's path. For example, to set up a local storage type, with the name `local` and path to the storage folder `/home/user/trace_archive`, run:

```bash
$ python trace_archive.py setup
Creating a new storage source.
Registred storage type options: local-storage
Select your storage type: local-storage
Enter your storage name: local
Enter the storage folder path: /home/user/trace_archive
```

All storage sources contains a type and a name. The type is used to identify the storage source, like `local-storage` or `google-drive`, while the name is used to identify the storage configuration in the tools commands.

With the initial setup done, you can add new storage sources or change the default storage source using the `setup` command again, with the commands `--add-storage` and `--set-default-storage`, respectively.

```bash
$ python trace_archive.py setup --add-storage
$ python trace_archive.py setup --set-default-storage
```

All configurations are stored in the `config.yaml` file, which is created in the current working directory when the `setup` command is run for the first time.

Checkout the [Creating and using second storage source](#creating-and-using-second-storage-source) section for more details on how to create and use a second storage source.

### Upload Command

The `upload` command is for upload a trace and it's workload. The a trace file, and if not presented in the storage yet, hte workload file. The tools also expects a metadata file, which is a YAML file with the name `<trace>.metadata.yaml`, where `<trace>` is the name of the trace file. Multiple traces can be uploaded at once, as long as they are from the same trace attempt.

The `upload` command options are:

* `--workload`: Path to the workload file.
* `--trace`: Path to the trace file.
* `--it`: Interactive files selection mode. If this option is used, the tool will prompt the user to select the workload and trace files 

For every upload, a unique [`trace-id`](#trace-id) will be generated and filled into the metadata file.

### List Command

The `list` command is used to list the available traces or workloads in the archive.

### Get Command

The `get` command is used to download a specified trace, workload or metadata file from the archive. The command options are:

* `--trace`: Id of the trace to download.
* `--workload`: Id of the workload to download. 
* `--metadata`: Id of the metadata (same as the trace id) to download.
* `-o, --output`: Output file path. If not specified, the file will be downloaded to the current working directory.

## Examples

Assuming the trace archive tool is set up with a local storage type named `local`, you can use the following commands:

### Uploading a Trace

To upload a trace file named `dhrystone.zstf` and its workload `dhrystone`, present in the metadata example folder, you can run:

```bash
$ python trace_archive.py --storage-name local upload --workload ../../stf_metadata/example/dhrystone --trace ../../stf_metadata/example/dhrystone.zstf

Uploading workload: ../../stf_metadata/example/dhrystone with id: 0
Uploading trace: ../../stf_metadata/example/dhrystone.zstf with id: 0.0.0000_dhrystone
```

### Downloading a Trace

To download the trace file `000.000.000_dhrystone.zstf` and its metadata, you can run:

```bash
$ python trace_archive.py get --trace 000.000.000_dhrystone.zstf

Trace 0.0.0000_dhrystone saved on ./0.0.0000_dhrystone.zstf
Metadata 0.0.0000_dhrystone saved on ./0.0.0000_dhrystone.zstf.metadata.yaml
```

### Downloading a Workload

To download the workload `0` (dhrystone), you can run:

```bash
$ python trace_archive.py get --workload 0

Workload 0 saved on ./dhrystone
```

### Creating and using second storage source

To create a second storage source you can run the `setup` command with the `--add-storage` option:

```bash
$ python trace_archive.py setup --add-storage
Creating a new storage source.
Registred storage type options: local-storage
Select your storage type: local-storage
Enter your storage name: private-storage
Enter the storage folder path: ./private
```

This will create a new storage source named `private-storage` with the path `./private`. You can then use this storage source in the `upload` command by specifying the `--storage` option:

```bash
$ python trace_archive.py --storage-name private-storage upload --workload ../../stf_metadata/example/dhrystone --trace ../../stf_metadata/example/dhrystone.zstf
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
| 1st      | `dhrystone` compiled with `-O3`, fully traced             | `0.0.0000_dhrystone` |
| 2nd      | `dhrystone` `-O3`, traced from instruction 0 to 1,000,000 | `0.1.0000_dhrystone` |
| 3rd      | `dhrystone` `-O3`, traced from 1,000,000 to 2,000,000     | `0.1.0001_dhrystone` |
| 4th      | `dhrystone` `-O3`, traced from 2,000,000 to 3,000,000     | `0.1.0002_dhrystone` |
| 5th      | Same trace as 1st (re-uploaded)                           | `0.2.0000_dhrystone` |
| 6th      | `dhrystone` compiled with `-O2`, fully traced             | `0.0.0000_dhrystone` |
| 7th      | `embench` compiled with `-O3`, fully traced               | `0.0.0000_embench`   |

---

## Storage Folder Structure

For the trace archive structure, each workload is stored in its own folder, identified by its workload_id. This folder contains the workload file, related outputs (e.g., stdout, objdump), and subdirectories for each trace attempt.

The tree graph below illustrates a setup of the [Trace Id Example](#example-trace-ids):

```text
0000_dhrystone/
├── dhrystone
├── dhrystone.objdump
├── dhrystone.stdout
├── attempt_0000/
│   ├── 0.0.0000_dhrystone.zstf
│   └── 0.0.0000_dhrystone.zstf.metadata.yaml
├── attempt_0001/
│   ├── 0.1.0000_dhrystone.zstf
│   ├── 0.1.0000_dhrystone.zstf.metadata.yaml
│   ├── 0.1.0001_dhrystone.zstf
│   ├── 0.1.0001_dhrystone.zstf.metadata.yaml
│   ├── 0.1.0002_dhrystone.zstf
│   └── 0.1.0002_dhrystone.zstf.metadata.yaml
0001_dhrystone/
├── dhrystone
├── dhrystone.objdump
├── dhrystone.stdout
└── attempt_0000/
    ├── 1.0.0000_dhrystone.zstf
    └── 1.0.0000_dhrystone.zstf.metadata.yaml
0002_embench/
├── embench.zip
├── embench.objdump
├── embench.stdout
└── attempt_0000/
    ├── 2.0.0000_embench.zstf
    └── 2.0.0000_embench.zstf.metadata.yaml

```

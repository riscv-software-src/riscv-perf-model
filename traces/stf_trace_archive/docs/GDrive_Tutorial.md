# Trace Archive Tool Google Drive Tutorial

## Table of Contents

## Introduction

Trace archive tool is a Python CLI application designed to help users upload, download and share STF trace and workload files. Fro more information, see the [Trace Archive README](../README.md).

To execute the tool, you can use the command `python trace_share.py COMMAND [OPTIONS]`. The full list of commands and options can be found in the [Trace Archive README](../README.md).

## 1. Initial Setup

The setup of the tool can be done in two steps:

1. Sync the desired Google Drive folder to your local machine.
1. Install the trace archive dependencies.
1. Creating / Updating the setup file to add the Google Drive storage source.

### 1.1 Sync Google Drive folder to local machine

There are multiple ways to sync a Google Drive folder to your local machine, as [Google Drive for Desktop](https://ipv4.google.com/drive/download/), [Gnome Online Accounts](https://help.gnome.org/users/gnome-help/stable/accounts.html.en) ,[rclone](https://rclone.org/drive/). This tutorial will focus on rclone, as it is a open source tool that works on Windows and Linux, if you wish to user another tool, you can skip this step and go to step [1.2](#12-install-the-trace-archive-dependencies).

To use rclone, the steps are as follows:

1. Install rclone by following the instructions [here](https://rclone.org/install/).
1. Configure rclone to access your Google Drive by following the instructions with the following command:

   ```bash
   rclone config
   ```

   Use this options when prompted:

   - `name`: drive
   - `client_id`: Leave empty to use rclone's default client ID.
   - `client_secret`: Leave empty to use rclone's default client secret.
   - `scope`: Select `drive` to have full access to your Google Drive.
   - `service_account_file`: Leave empty unless you are using a service account.
   - `auto config`: Set to `true` if you are running rclone on a machine with a web browser. If you are running rclone on a headless server, set to `false` and follow the instructions to authenticate.

1. Mount the [trace archive drive folder](https://drive.google.com/drive/folders/1c2-eC7FfiSAFXK_g-6L1YQvBaDmmF_0s?usp=sharing) to a local folder.
   - Create a local folder to mount the Google Drive folder, e.g. `mkdir ~/trace_archive`.
   - Run the command
     ```bash
     rclone mount --drive-shared-with-me "drive:Workgroups/Performance Modeling/trace_repository"  ~/trace_archive
     ```

### 1.2 Install the trace archive dependencies

To install the trace archive dependencies, run the following command:

```bash
cd traces/stf_trace_archive/src
apt install python3-pip
pip install -r requirements.txt
```

### 1.3 Create / Update the trace archive setup file

The setup file is a JSON file that contains the configuration for the trace archive tool, to create it, run the following command:

```bash
python3 trace_archive.py setup
```

You will be prompted with a few questions. Follow the example below, typing the answers when asked.

```console
Config is empty or invalid. Creating new config file
Creating a new storage source.
Registred storage type options: local-storage

Select your storage type: local-storage   # ← type: local-storage
Enter your storage name: drive            # ← type: drive
Enter the storage folder path: ~/trace-archive   # ← type: ~/trace-archive
```

## Uploading a Trace

To upload the trace and workload examples provided in the [stf_metadata example folder](../../stf_metadata/example), run the following command:

```bash
python3 trace_archive.py upload --workload ../../stf_metadata/example/dhrystone --trace ../../stf_metadata/example/dhrystone.zstf
```

this should produce the following output:

```console
Uploading workload: ../../stf_metadata/example/dhrystone with id: 0
Uploading trace: ../../stf_metadata/example/dhrystone.zstf with id: 0.0.0000_dhrystone
```

And the files should be visible in the Google Drive folder.

## Downloading a Trace

To download a trace, you can use the `list` command to see the available traces and then use the `get` command to download the desired trace. For example, to list the available traces, run the following command:

```bash
$ python3 trace_archive.py list
```

```console
id: 0.0.0000_dhrystone
Workload: dhrystone
Trace Timestamp: 2025-07-20T21:05:32.840053+00:00
Fully trace
```

To download the trace with id `0.0.0000_dhrystone`, run the following command:

```bash
python3 trace_archive.py get --trace 0.0.0000_dhrystone -o dhrystone_trace.zstf
```

```console
Trace 0.0.0000_dhrystone saved on /home/ribeiro/mine-riscv-perf-model/riscv-perf-model/traces/stf_trace_archive/src/dhrystone_trace.zstf
Metadata 0.0.0000_dhrystone saved on /home/ribeiro/mine-riscv-perf-model/riscv-perf-model/traces/stf_trace_archive/src/dhrystone_trace.zstf.metadata.yaml
```

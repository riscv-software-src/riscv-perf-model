# Conda enviornments

The conda environment created using yml file specification in this
directory can be used to build and run olympia, the [Sparta Modeling
Framework](https://github.com/sparcians/map/tree/master/sparta), [STF
Library](https://github.com/sparcians/stf_lib) and [STF
Tools](https://github.com/sparcians/stf_tools).

## Download and install miniconda

If `conda` is not already set up or the preference is to have a local
`conda` environment, consider installing [miniconda](https://docs.conda.io/en/latest/miniconda.html) first.

## Create a New Environment or Update Existing Conda Environment

**If these steps do no work, follow the directions on
  [Map/Sparta](https://github.com/sparcians/map/tree/master#building-map)
  instead**

A new environment can be created using `environment.yaml` file as follows:

```
conda env create -f environment.yml
```

This will create a conda environment named `riscv_perf_model` in the default conda path.

If an environment named `riscv_perf_model` exists, it can be updated as follows:

```
conda env update --file environment.yml  --prune
```

The file `environment_for_macos.yml` provides an alternative specification if encountering problems
while building the conda environement for macOS.

The file `environment_from_history.yml` provides a minimal specification, in case, there is a need
to build the conda environment from scratch.

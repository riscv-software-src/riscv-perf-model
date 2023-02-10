# Conda enviornments

The conda environment created using yml file specification in this directory can be used to build and run olympia, stf`_`lib and stf`_`tools.

## Create new environment or update existing environment

A new environment can be created using `environment.yaml` file as follows:

```
conda env create -f environment.yml
```

This will create a conda environment named `riscv_perf_model` in the default conda path.

If an environment named `riscv_perf_model` exists, it can be updated as follows:

```
conda env update --file environment.yml  --prune
```

Thw file `environment_from_history.yml` provides a minimal specification, in case, there is a need to build the conda environment from scratch.

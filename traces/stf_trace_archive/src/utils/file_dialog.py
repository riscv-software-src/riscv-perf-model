from tkinter import filedialog

class FileDialog():

    @staticmethod
    def select_workload() -> str:
        file_path = filedialog.askopenfilename(title="Select Workload")
        return file_path    

    @staticmethod
    def select_traces() -> list[str]:
        file_paths = filedialog.askopenfilenames(title="Select Traces", filetypes=[("ZSTF", ".zstf")])
        return file_paths
from contextlib import ExitStack
import io
import os
import re
import unittest

from unittest.mock import patch
from tests.utils.trace_generator import TraceDataGenerator
from data.consts import Const
from trace_share import main
import sys

class TraceTestInput():
    def __init__(self, workload_path, trace_paths):
        self.workload_path = workload_path
        self.trace_paths = trace_paths

class TestUpload(unittest.TestCase):
    def setUp(self):
        self.generator = TraceDataGenerator()
        self.workload1_path = self.generator.generate_worload(0)
        self.workload1_name = self.workload1_path.split('/')[-1]
        self.storage_type = "local-storage"
        self.storage_path = "./tests/storage_test"

        self.workload1_full_trace_path = self.generator.generate_trace(workload_id=self.workload1_path, trace_attempt=0, trace_part=None)
        self.workload1_trace_part_1_path = self.generator.generate_trace(workload_id=self.workload1_path, trace_attempt=1, trace_part=0)
        self.workload1_trace_part_2_path = self.generator.generate_trace(workload_id=self.workload1_path, trace_attempt=1, trace_part=1)
        self.default_args = [
            "trace_share", 
            "--source-type",
            self.storage_type,
            "--source-path",
            self.storage_path,
            "upload",
        ]

    def tearDown(self):
        self.generator.delete_test_traces()
        self.generator.delete_test_storage(self.storage_type, self.storage_path)
        pass

    def launch_test(self, workload, traces, inputs = None):
        args = [
            *self.default_args,
        ]

        if workload:
            args.append("--workload")
            args.append(workload)

        if traces:
            if isinstance(traces, str):
                traces = [traces]
            for trace in traces:
                args.append("--trace")
                args.append(trace)

        captured_output = io.StringIO()
        captured_stderr = io.StringIO()
        try:
            with ExitStack() as stack:
                stack.enter_context(patch.object(sys, 'argv', args))
                stack.enter_context(patch("sys.stdout", new=captured_output))
                stack.enter_context(patch("sys.stderr", new=captured_stderr))
                if inputs is not None:
                    stack.enter_context(patch('builtins.input', side_effect = inputs))

                main()
        except Exception as e:
            # print("stdout")
            # print(captured_output.getvalue())
            # print("stderr")
            # print(captured_stderr.getvalue())
            # print("e")
            # print(e)
            # traceback.print_exc()
            return captured_output.getvalue(), captured_stderr.getvalue(), e
            
        # print("stdout")
        # print(captured_output.getvalue())
        return captured_output.getvalue(), None, None

    def trace_exists_assert(self, trace_id, workload_name):
        workload_id, attempt_id = trace_id.split(".")[0:2]
        workload_folder = f"{workload_id.zfill(Const.PAD_LENGHT)}_{workload_name}"
        attempt_folder = f"attempt_{attempt_id.zfill(Const.PAD_LENGHT)}"
        trace_path = os.path.join(self.storage_path, workload_folder, attempt_folder, f"{trace_id}.zstf")
        metadata_path = f"{trace_path}.metadata.yaml"

        self.assertTrue(os.path.exists(trace_path))
        self.assertTrue(os.path.exists(metadata_path))

    def get_trace_ids_from_output(self, output):
        if not output:
            return []
        
        pattern = r"(?<=\s)\d+\.\d+\.\d+_\S+"
        return re.findall(pattern, output)

    def test_upload_full_trace(self):
        print(f"\ntest_upload_full_trace")

        stdout, stderr, error = self.launch_test(self.workload1_path, self.workload1_full_trace_path)
        trace_ids = self.get_trace_ids_from_output(stdout)
        expected_trace_id = f"0.0.0000_{self.workload1_name}"

        self.assertIsNone(error)
        self.assertIsNone(stderr)
        self.assertEqual(len(trace_ids), 1)
        self.assertEqual(trace_ids[0], expected_trace_id)
        self.trace_exists_assert(expected_trace_id, self.workload1_name)

    def test_upload_partial_traces(self):
        print(f"\n\ntest_upload_partial_trace")
        inputs = ["0", "1"]
        stdout, stderr, error = self.launch_test(self.workload1_path, [self.workload1_trace_part_1_path, self.workload1_trace_part_2_path], inputs)
        trace_ids = self.get_trace_ids_from_output(stdout)
        expected_trace_ids = [f"0.0.0000_{self.workload1_name}", f"0.0.0001_{self.workload1_name}"]

        self.assertIsNone(error)
        self.assertIsNone(stderr)
        self.assertEqual(len(trace_ids), len(expected_trace_ids))
        for i in range(0, len(trace_ids)):
            self.assertEqual(trace_ids[i], expected_trace_ids[i], self.workload1_name)
            self.trace_exists_assert(expected_trace_ids[i], self.workload1_name)

    def test_upload_two_attempts(self):
        print(f"\n\ntest_upload_two_attempts")
        stdout1, stderr1, error1 = self.launch_test(self.workload1_path, self.workload1_full_trace_path)
        
        inputs = ["y", "0", "1"]
        stdout2, stderr2, error2 = self.launch_test(self.workload1_path, [self.workload1_trace_part_1_path, self.workload1_trace_part_2_path], inputs)

        trace_ids = self.get_trace_ids_from_output(stdout1)
        trace_ids.extend(self.get_trace_ids_from_output(stdout2))
        expected_trace_ids = [f"0.0.0000_{self.workload1_name}", f"0.1.0000_{self.workload1_name}", f"0.1.0001_{self.workload1_name}"]

        self.assertIsNone(error1)
        self.assertIsNone(error2)
        self.assertIsNone(stderr1)
        self.assertIsNone(stderr2)
        self.assertEqual(len(trace_ids), len(expected_trace_ids))
        for i in range(0, len(trace_ids)):
            self.assertEqual(trace_ids[i], expected_trace_ids[i], self.workload1_name)
            self.trace_exists_assert(expected_trace_ids[i], self.workload1_name)

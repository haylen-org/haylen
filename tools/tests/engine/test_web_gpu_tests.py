import pytest

from haylen.core.errors.build_failed import BuildFailed
from haylen.engine.web_gpu_tests import WebGpuTests


class WebGpuTestsTest:
    """The GPU tests of the web pass only when their page finished with the status 0 and never stopped or threw, before the tests finished or after."""

    STOP = "The GPU tests stopped: Aborted(Assertion failed: native function `strerror` called after runtime exit (use NO_EXIT_RUNTIME to keep it alive after main() exits))"

    def test_tests_that_finished_with_the_status_0_in_a_page_that_never_stopped_pass(self):
        WebGpuTests.check({"status": 0, "failures": []})

    def test_a_page_that_stops_after_the_tests_finished_fails_the_run(self):
        with pytest.raises(BuildFailed, match="The page of the GPU tests of the web stopped with") as error:
            WebGpuTests.check({"status": 0, "failures": [WebGpuTestsTest.STOP, WebGpuTestsTest.STOP]})
        assert "called after runtime exit" in str(error.value)

    def test_tests_that_failed_or_did_not_finish_fail_the_run(self):
        with pytest.raises(BuildFailed, match="failed with the status 1"):
            WebGpuTests.check({"status": 1, "failures": []})
        with pytest.raises(BuildFailed, match=f"did not finish within {WebGpuTests.SECONDS} seconds"):
            WebGpuTests.check({"status": None, "failures": []})

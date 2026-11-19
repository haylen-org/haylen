import json
import subprocess
import sys

from haylen.core.repository import Repository


class CompilerCacheReportTest:
    """The report of the compiler cache in the jobs of CI counts the compiles that came from the cache and the ones that compiled."""

    SCRIPT = Repository.ROOT / ".github" / "actions" / "compiler-cache" / "report.py"

    @staticmethod
    def report(hits: int, misses: int, failures: int) -> str:
        duration = {"secs": 1, "nanos": 0}
        stats = {
            "cache_hits": {"counts": {"C/C++": hits}},
            "cache_misses": {"counts": {"C/C++": misses}},
            "compile_fails": failures,
            "requests_not_cacheable": 0,
            "not_cached": {},
            "cache_read_hit_duration": duration,
            "compiler_write_duration": duration,
        }
        result = subprocess.run([sys.executable, str(CompilerCacheReportTest.SCRIPT)], input=json.dumps({"stats": stats}), capture_output=True, text=True, check=True)
        return result.stdout

    def test_counts_the_failed_probes_apart_from_the_misses(self):
        report = CompilerCacheReportTest.report(hits=2557, misses=0, failures=14)

        assert "2557 of 2557 compiles came from the cache (100.00 %), and 0 compiled" in report
        assert "14 compiles failed" in report

    def test_counts_every_miss_as_a_compile(self):
        report = CompilerCacheReportTest.report(hits=2033, misses=402, failures=28)

        assert "2033 of 2435 compiles came from the cache (83.49 %), and 402 compiled" in report

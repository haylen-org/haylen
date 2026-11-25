import re

from haylen.core.repository import Repository
from tests.support.tracked_files import TrackedFiles


class TestLinkTest:
    """The two test programs link one at a time, since a link of each takes gigabytes of memory under the sanitizers and two side by side exhaust a machine of continuous integration."""

    PROGRAMS = ("haylen-tests", "haylen-lua-tests")
    POOL = re.compile(r"\bJOB_POOLS\s+([a-z0-9-]+)=([0-9]+)")
    PROPERTIES = re.compile(r"\bset_target_properties\s*\(\s*([a-z0-9-]+)\s+PROPERTIES([^)]*)\)")

    def test_the_test_programs_link_in_a_pool_of_one_job(self):
        text = "\n".join((Repository.ROOT / name).read_text(encoding="utf-8") for name in TrackedFiles.names("*CMakeLists.txt", "*.cmake"))
        pools = {match.group(1): int(match.group(2)) for match in TestLinkTest.POOL.finditer(text)}
        linked = {}
        for match in TestLinkTest.PROPERTIES.finditer(text):
            pool = re.search(r"\bJOB_POOL_LINK\s+([a-z0-9-]+)", match.group(2))
            if pool:
                linked[match.group(1)] = pool.group(1)
        assert {program: pools.get(linked.get(program, "")) for program in TestLinkTest.PROGRAMS} == dict.fromkeys(TestLinkTest.PROGRAMS, 1)

import pytest

from haylen.core.errors.build_failed import BuildFailed
from haylen.engine.web_checks import WebChecks


class WebMemoryTest:
    """The memory check of the web stops on a page that failed, missed the download or holds memory that grows with what it downloads, and on a page without the files of the browser that misses its room."""

    def test_stops_on_memory_that_grows_with_the_download(self):
        good = {"failure": None, "finished": True, "receivedBytes": WebChecks.MEMORY_FILE_SIZE + 1, "peakWasmBytes": 128 * 1024**2, "buffersAfterCollection": 16 * 1024**2, "log": []}
        WebChecks.check_memory(good)
        changes = (
            ({"peakWasmBytes": WebChecks.MEMORY_FILE_SIZE}, "grows with what it downloads"),
            ({"buffersAfterCollection": 128 * 1024**2}, "array buffers"),
            ({"receivedBytes": 1024}, "fewer than"),
            ({"finished": False}, "did not finish"),
            ({"failure": "Broken."}, "failed: Broken."),
        )
        for change, message in changes:
            with pytest.raises(BuildFailed, match=message):
                WebChecks.check_memory({**good, **change})

    def test_stops_on_a_page_without_files_that_misses_its_room(self):
        refused = {"downloadedMedium": True, "finished": False, "failure": "The content needs 486613920 bytes of room on the device and 251656020 are free."}
        WebChecks.check_memory_without_files(refused)
        changes = (
            ({"downloadedMedium": False}, "did not download the medium file"),
            ({"finished": True, "failure": None}, "downloaded it"),
            ({"failure": "The network failed."}, "reported: The network failed."),
        )
        for change, message in changes:
            with pytest.raises(BuildFailed, match=message):
                WebChecks.check_memory_without_files({**refused, **change})

import os
import subprocess

from haylen_ui_test_case import HaylenUiTestCase


class LinkTests(HaylenUiTestCase):
    """A link of the URL scheme of the plugin "native-demo" starts a second process of the app, as Windows does for every link, which hands the link to the app that runs through its named pipe and ends, and the app that runs hears it as "linkOpened" in the test "Event checklist"."""

    launch_arguments = ("--test", "PLT-006")
    LINK = "haylendemo://hello"

    def test_a_link_reaches_the_app_that_runs(self) -> None:
        self.wait_for("linkOpened")
        second = subprocess.run([os.environ["HAYLEN_UI_APP"], "--link", LinkTests.LINK], stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=HaylenUiTestCase.TIMEOUT)
        assert second.returncode == 0, f"The second process of the link ended with the status {second.returncode} and the output {second.stdout}{second.stderr}."
        self.wait_for(f"The last one came with url {LinkTests.LINK}.")
        self.save_screenshot("windows-link-opened")

from haylen_ui_test_case import HaylenUiTestCase


class ErrorScreenTests(HaylenUiTestCase):
    """The error screen of the test "Errors and recovery" reads to UI Automation as Narrator reads it: its title, the message and the place of the error and its buttons, and its button "Back to the app" goes back to the test."""

    launch_arguments = ("--test", "DEV-007")
    TITLE = "The app stopped with an error"
    MESSAGE = "The update of the test failed."

    def test_the_error_screen_reads_its_texts_and_buttons(self) -> None:
        self.click("Raise")
        self.wait_for(ErrorScreenTests.TITLE)
        self.wait_for(ErrorScreenTests.MESSAGE)
        for button in ("Copy report", "Restart app", "Back to the app"):
            self.wait_for(button)
        self.save_screenshot("windows-error-screen")
        self.click("Back to the app")
        self.wait_for("Errors and recovery")

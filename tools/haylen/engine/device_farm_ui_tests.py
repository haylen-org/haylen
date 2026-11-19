import json
import time
import urllib.request
from collections.abc import Sequence
from pathlib import Path

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.folders import Folders
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext


class DeviceFarmUiTests:
    """Runs the UI tests of an app on real phones that AWS Device Farm rents, in one run of a project that it creates for the run and deletes afterwards, whatever the run ends with, and then checks that the project is gone. The AWS command line takes the credentials from the environment, as `AWS_ACCESS_KEY_ID` and `AWS_SECRET_ACCESS_KEY`, so no key reaches a file or a command line. Each device runs the app and its runner once, bounded by the minutes of a job, and the run copies back the files that Device Farm keeps of every job, such as the log of the device and the video of the run."""

    REGION = "us-west-2"
    # The type of the upload of the app and of the runner, and the type of the test, of each platform.
    TYPES = {"android": ("ANDROID_APP", "INSTRUMENTATION_TEST_PACKAGE", "INSTRUMENTATION"), "ios": ("IOS_APP", "XCTEST_UI_TEST_PACKAGE", "XCTEST_UI")}
    PLATFORMS = {"android": "ANDROID", "ios": "IOS"}
    UPLOAD_SECONDS = 900
    # The longest that one device runs the tests, which Device Farm bills by the minute.
    JOB_MINUTES = 30
    POLL_SECONDS = 15

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    def aws(self, *arguments: object) -> dict:
        """Runs an action of Device Farm with the AWS command line and returns its JSON answer."""
        command: list[object] = ["aws", "devicefarm", *arguments, "--region", DeviceFarmUiTests.REGION, "--output", "json"]
        answer = self.context.process.capture(command)
        return json.loads(answer) if answer.strip() else {}

    @staticmethod
    def selection(devices: Sequence[dict], names: Sequence[str]) -> list[str]:
        """The identifiers of the devices that Device Farm keeps available whose names the request lists, one for each name, and a sentence that lists the names it has for a name it lacks."""
        chosen = []
        for name in names:
            matches = [device for device in devices if device["name"] == name and device.get("availability") in ("HIGHLY_AVAILABLE", "AVAILABLE")]
            if not matches:
                offered = ", ".join(sorted({f'"{device["name"]}"' for device in devices}))
                raise UnsupportedTarget(f'Device Farm has no available device named "{name}". It offers {offered}.')
            newest = max(matches, key=lambda device: [int(part) for part in device["os"].split(".") if part.isdigit()])
            chosen.append(newest["arn"])
        return chosen

    def run(self, platform: str, app: Path, runner: Path, names: Sequence[str], output: Path) -> dict[str, object]:
        if platform not in DeviceFarmUiTests.TYPES:
            raise UnsupportedTarget('Device Farm runs the UI tests of "android" and "ios". Pick one with "--target".')
        app_type, runner_type, test_type = DeviceFarmUiTests.TYPES[platform]
        listed = self.aws("list-devices", "--filters", json.dumps([{"attribute": "PLATFORM", "operator": "EQUALS", "values": [DeviceFarmUiTests.PLATFORMS[platform]]}]))
        devices = DeviceFarmUiTests.selection(listed["devices"], names)

        self.context.terminal.step("Creating a project of Device Farm for the run")
        project = self.aws("create-project", "--name", f"haylen-ui-tests-{int(time.time())}")["project"]["arn"]
        try:
            app_upload = self.upload(project, app, app_type)
            runner_upload = self.upload(project, runner, runner_type)
            selection = {"filters": [{"attribute": "ARN", "operator": "IN", "values": devices}], "maxDevices": len(devices)}
            self.context.terminal.step(f"Running the UI tests on {len(devices)} devices of Device Farm")
            scheduled = self.aws(
                "schedule-run",
                "--project-arn",
                project,
                "--app-arn",
                app_upload,
                "--device-selection-configuration",
                json.dumps(selection),
                "--test",
                json.dumps({"type": test_type, "testPackageArn": runner_upload}),
                "--execution-configuration",
                json.dumps({"jobTimeoutMinutes": DeviceFarmUiTests.JOB_MINUTES}),
            )
            results = self.follow(scheduled["run"]["arn"], len(devices), output)
        finally:
            self.delete(project)

        failed = [job for job in results if job["result"] != "PASSED"]
        if failed:
            lines = "\n".join(f'- "{job["device"]}": {job["result"]}. {job["message"]}' for job in failed)
            raise BuildFailed(f"The UI tests failed on Device Farm, and the files of each job are in `{Repository.shown(output)}`:\n{lines}")
        self.context.terminal.success(f"The UI tests passed on {len(results)} devices of Device Farm.")
        return {"target": platform, "devices": [job["device"] for job in results], "output": str(output)}

    def upload(self, project: str, path: Path, kind: str) -> str:
        """Uploads a file to the project through the address that Device Farm signs for it and waits until Device Farm read it."""
        self.context.terminal.step(f"Uploading `{path.name}` to Device Farm")
        created = self.aws("create-upload", "--project-arn", project, "--name", path.name, "--type", kind, "--content-type", "application/octet-stream")["upload"]
        request = urllib.request.Request(created["url"], data=path.read_bytes(), method="PUT", headers={"Content-Type": "application/octet-stream"})
        with urllib.request.urlopen(request) as response:
            response.read()
        deadline = time.monotonic() + DeviceFarmUiTests.UPLOAD_SECONDS
        while True:
            upload = self.aws("get-upload", "--arn", created["arn"])["upload"]
            if upload["status"] == "SUCCEEDED":
                return created["arn"]
            if upload["status"] == "FAILED":
                raise BuildFailed(f"Device Farm refused `{path.name}`: {upload.get('metadata') or upload.get('message', 'it gave no reason')}")
            if time.monotonic() > deadline:
                raise BuildFailed(f"Device Farm did not read `{path.name}` within {DeviceFarmUiTests.UPLOAD_SECONDS} seconds.")
            time.sleep(DeviceFarmUiTests.POLL_SECONDS)

    def follow(self, run: str, devices: int, output: Path) -> list[dict[str, str]]:
        """Waits until the run completes, which the minutes of its jobs bound, prints what each job ended with and copies the files of every job into the output folder."""
        deadline = time.monotonic() + DeviceFarmUiTests.JOB_MINUTES * 60 * devices + DeviceFarmUiTests.UPLOAD_SECONDS
        status = ""
        while status != "COMPLETED":
            if time.monotonic() > deadline:
                self.aws("stop-run", "--arn", run)
                raise BuildFailed("The run of Device Farm did not complete within the minutes of its jobs, so the run stopped it.")
            time.sleep(DeviceFarmUiTests.POLL_SECONDS)
            status = self.aws("get-run", "--arn", run)["run"]["status"]

        Folders.remove(output)
        output.mkdir(parents=True)
        results = []
        for job in self.aws("list-jobs", "--arn", run)["jobs"]:
            name = job["device"]["name"]
            message = job.get("message", "")
            folder = output / DeviceFarmUiTests.folder_name(name)
            failures = self.failures(job["arn"], folder)
            results.append({"device": name, "result": job["result"], "message": " ".join([message, *failures]).strip()})
            self.context.terminal.info(f'"{name}" ended with {job["result"]}.')
            self.download(job["arn"], folder)
        return results

    @staticmethod
    def folder_name(name: str) -> str:
        """The name of the folder of a device or a test, in lowercase with dashes."""
        return name.replace(" ", "-").lower()

    def failures(self, job: str, folder: Path) -> list[str]:
        """The tests of a job that did not pass, with the message that each one failed with. Device Farm runs each test on its own, so the files of the job hold the output of its last test alone, and the files of each test that did not pass go into a folder of their own."""
        found = []
        for suite in self.aws("list-suites", "--arn", job)["suites"]:
            for test in self.aws("list-tests", "--arn", suite["arn"])["tests"]:
                if test["result"] in ("PASSED", "SKIPPED"):
                    continue
                found.append(f'The test "{test["name"]}" ended with {test["result"]}: {test.get("message", "")}')
                self.download(test["arn"], folder / DeviceFarmUiTests.folder_name(test["name"]))
        return found

    def download(self, job: str, folder: Path) -> None:
        folder.mkdir(parents=True, exist_ok=True)
        for artifact in self.aws("list-artifacts", "--arn", job, "--type", "FILE")["artifacts"]:
            target = folder / f"{artifact['name'].replace(' ', '-').lower()}.{artifact['extension']}"
            with urllib.request.urlopen(artifact["url"]) as response:
                target.write_bytes(response.read())

    def delete(self, project: str) -> None:
        """Deletes the project with its uploads and its run, and checks that Device Farm no longer lists it."""
        self.context.terminal.step("Deleting the project of Device Farm")
        self.aws("delete-project", "--arn", project)
        remaining = [listed["arn"] for listed in self.aws("list-projects")["projects"]]
        if project in remaining:
            raise BuildFailed(
                f'The project "{project}" of Device Farm is still there after its deletion. Delete it with "aws devicefarm delete-project --arn {project} --region {DeviceFarmUiTests.REGION}".'
            )

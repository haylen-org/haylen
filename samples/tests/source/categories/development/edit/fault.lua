-- The module of the test "DEV-005". Once the test armed the error, "fault.step" raises on every frame. Delete the line that raises and save the file: the app resumes from the error screen, and the test keeps counting from where it stopped.
local fault = {}

function fault.step(armed)
    if armed then
        error('The error of the test DEV-005. Delete this line and save the file to resume the app.')
    end
end

return fault

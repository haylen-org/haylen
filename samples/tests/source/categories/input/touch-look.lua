-- The look of the touch controls of the input tests: a style whose SVG surfaces of `content/input/` draw the touch stick as a glassy base with a raised knob and the touch buttons as round buttons that sink when pressed, outlined like the art of the project.
local touchLook = {}

touchLook.style = {surfaces = {
    stickBase = {image = 'input/touch-stick-base.svg'},
    stickKnob = {image = 'input/touch-stick-knob.svg'},
    touchButton = {image = 'input/touch-button.svg'},
    touchButtonPressed = {image = 'input/touch-button-pressed.svg'},
}}

return touchLook

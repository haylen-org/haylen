-- The look of the touch controls of the input tests: a style whose vector surfaces draw the touch stick as a glassy base with a raised knob and the touch buttons as round buttons that sink when pressed, outlined like the art of the project.
local touchLook = {}

touchLook.base = [[<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 200">
<defs><radialGradient id="fill" cx="0.5" cy="0.4" r="0.6"><stop offset="0" stop-color="#4A5C96"/><stop offset="1" stop-color="#1E2648"/></radialGradient></defs>
<circle cx="100" cy="100" r="94" fill="url(#fill)" fill-opacity="0.85" stroke="#15172A" stroke-width="6"/>
<circle cx="100" cy="100" r="76" fill="none" stroke="#8FB0FF" stroke-opacity="0.45" stroke-width="3" stroke-dasharray="7 9"/>
<path d="M100 16 L90 30 H110 Z M184 100 L170 90 V110 Z M100 184 L90 170 H110 Z M16 100 L30 90 V110 Z" fill="#C9D6FF" fill-opacity="0.8"/>
</svg>]]

touchLook.knob = [[<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
<defs><radialGradient id="fill" cx="0.4" cy="0.32" r="0.72"><stop offset="0" stop-color="#FFFFFF"/><stop offset="0.5" stop-color="#C9D6FF"/><stop offset="1" stop-color="#6F86D0"/></radialGradient></defs>
<circle cx="50" cy="55" r="42" fill="#15172A" fill-opacity="0.45"/>
<circle cx="50" cy="50" r="42" fill="url(#fill)" stroke="#15172A" stroke-width="5"/>
<ellipse cx="38" cy="33" rx="14" ry="8" fill="#FFFFFF" fill-opacity="0.65"/>
</svg>]]

touchLook.button = [[<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 120 120">
<defs><radialGradient id="fill" cx="0.5" cy="0.32" r="0.68"><stop offset="0" stop-color="#5C70B4"/><stop offset="1" stop-color="#252C55"/></radialGradient></defs>
<circle cx="60" cy="64" r="53" fill="#15172A" fill-opacity="0.5"/>
<circle cx="60" cy="59" r="53" fill="url(#fill)" fill-opacity="0.92" stroke="#15172A" stroke-width="6"/>
<path d="M26 44 A38 38 0 0 1 94 44" fill="none" stroke="#FFFFFF" stroke-opacity="0.28" stroke-width="6" stroke-linecap="round"/>
</svg>]]

touchLook.pressed = [[<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 120 120">
<defs><radialGradient id="fill" cx="0.5" cy="0.36" r="0.68"><stop offset="0" stop-color="#A9C2FF"/><stop offset="1" stop-color="#4560C8"/></radialGradient></defs>
<circle cx="60" cy="64" r="53" fill="url(#fill)" stroke="#15172A" stroke-width="6"/>
<path d="M28 50 A36 36 0 0 1 92 50" fill="none" stroke="#FFFFFF" stroke-opacity="0.35" stroke-width="5" stroke-linecap="round"/>
</svg>]]

touchLook.style = {surfaces = {
    stickBase = {image = touchLook.base},
    stickKnob = {image = touchLook.knob},
    touchButton = {image = touchLook.button},
    touchButtonPressed = {image = touchLook.pressed},
}}

return touchLook

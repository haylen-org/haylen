-- The tests of the category in menu order.
return {
    prefix = 'ACC',
    title = 'Accessibility',
    description = 'The accessibility settings of the system and what follows them, the elements that screen readers read with their actions and announcements, and the deficiencies of color vision.',
    tests = {
        {code = 'ACC-001', title = 'Accessibility settings', description = 'The text scale, reduced motion, high contrast, reduced transparency and screen reader of the system, live, with the UI and the motion that follow them.', module = 'settings'},
        {code = 'ACC-002', title = 'Screen reader elements', description = 'The elements a screen reader reads of a form, with their roles, labels, values and states, actions performed the way a screen reader asks for them, and announcements.', module = 'screen-reader'},
        {code = 'ACC-003', title = 'Color vision', description = 'Colors that tell things apart, as protanopia, deuteranopia and tritanopia see them, and corrected for each.', module = 'color-vision'},
    },
}

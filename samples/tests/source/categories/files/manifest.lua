-- The tests of the category in menu order.
return {
    prefix = 'FIL',
    title = 'Files and storage',
    description = 'The files of the package, the user folder through "haylen.storage" and Varn "fs", save slots, zip archives and a browser of the user folder.',
    tests = {
        {code = 'FIL-001', title = 'Package files', description = 'Text, JSON and binary files read from the content folder of the package.', module = 'package'},
        {code = 'FIL-002', title = 'User folder with storage', description = 'Write, read, append, list, "exists", sizes and remove, synchronously with "haylen.storage".', module = 'storage'},
        {code = 'FIL-003', title = 'User folder with fs', description = 'Folders, files, streams, copies and removals, asynchronously with Varn "fs" on the I/O pool.', module = 'fs'},
        {code = 'FIL-004', title = 'Save slots', description = 'Saving, loading and deleting named slots with the summaries a load menu shows.', module = 'save-slots'},
        {code = 'FIL-005', title = 'Zip archives', description = 'Creating, listing and extracting zip archives with Varn "zip", including one shipped in the package.', module = 'zip'},
        {code = 'FIL-006', title = 'File browser', description = 'The folders and files of the user folder, with sizes, dates and a preview.', module = 'browser'},
    },
}

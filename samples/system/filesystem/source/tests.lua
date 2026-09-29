-- The tests of the sample in menu order. Each module returns a scene class that takes its entry.
return {
    {id = 'package', title = 'Package files', description = 'Text, JSON and binary files read from the content folder of the package.', module = 'tests.package'},
    {id = 'storage', title = 'User folder with storage', description = 'Write, read, append, list, exists, sizes and remove, synchronously with haylen.storage.', module = 'tests.storage'},
    {id = 'fs', title = 'User folder with fs', description = 'Folders, files, streams, copies and removals, asynchronously with Varn fs on the I/O pool.', module = 'tests.fs'},
    {id = 'save-slots', title = 'Save slots', description = 'Saving, loading and deleting named slots with the summaries a load menu shows.', module = 'tests.save-slots'},
    {id = 'zip', title = 'Zip archives', description = 'Creating, listing and extracting zip archives with Varn zip, including one shipped in the package.', module = 'tests.zip'},
    {id = 'browser', title = 'File browser', description = 'The folders and files of the user folder, with sizes, dates and a preview.', module = 'tests.browser'},
}

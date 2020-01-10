
from: http://jungels.net/articles/diff-patch-ten-minutes.html

The Ten Minute Guide to diff and patch

Situation one: you are trying to compile a package from source and you discover that somebody has already done the work for you of modifying it slightly to compile on your system. They have made their work available as a "patch", but you're not sure how to make use of it. The answer is that you apply the patch to the original source code with a command line tool called, appropriately, patch.

Situation two: you have downloaded the source code to an open source package and after an hour or so of minor edits, you manage to make it compile on your system. You would like to make your work available to other programmers, or to the authors of the package, without redistributing the entire modified package. Now you are in a situation where you need to create a patch of your own, and the tool you need is diff.

This is a quick guide to diff and patch which will help you in these situations by describing the tools as they are most commonly used. It tells you enough to get started right away. Later, you can learn the ins and outs of diff and patch at your leisure, using the man pages.
Applying patches with patch

To apply a patch to a single file, change to the directory where the file is located and call patch:

patch < foo.patch

These instructions assume the patch is distributed in unified format, which identifies the file the patch should be applied to. If not, you can specify the file on the command line:

patch foo.txt < bar.patch

Applying patches to entire directories (perhaps the more common case) is similar, but you have to be careful about setting a "p level". What this means is that, within patch files, the files to be patched are identified by path names which may be different now that the files are located on your computer rather than on the computer where the patch was created. The p level instructs patch to ignore parts of the path name so that it can identify the files correctly. Most often a p level of one will work, so you use:

patch -p1 < baz.patch

You should change to the top level source directory before running this command. If a patch level of one does not correctly identify any files to patch, inspect the patch file for file names. If you see a name like

/users/stephen/package/src/net/http.c

and you are working in a directory that contains net/http.c, use

patch -p5 < baz.patch

In general, count up one for each path separator (slash character) that you remove from the beginning of the path, until what's left is a path that exists in your working directory. The count you reach is the p level.

To remove a patch, use the -R flag, ie

patch -p5 -R < baz.patch

Creating patches with diff

Using diff is simple whether you are working with single files or entire source directories. To create a patch for a single file, use the form:

diff -u original.c new.c > original.patch

To create a patch for an entire source tree, make a copy of the tree:

cp -R original new

Make any changes required in the directory new/. Then create a patch with the following command:

diff -rupN original/ new/ > original.patch

That's all you need to get started with diff and patch. For more information use:

man diff
man patch

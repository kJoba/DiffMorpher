DiffMorpher
==========
Commandline tool to use the Diff Match and Patch librarie to apply patches generated from two files/folders onto another file/folder

In order to use with Qt5 and UTF8, it's recommended to change *char* to *QChar* in Diff Match and Patch library, thus getting rid of Qt4s deprecated .toAscii() method.
You can accomplish this by using the diff_match_patch.cpp.patch file: `patch -u Libs/diff-match-patch/cpp/diff_match_patch.cpp -i diff_match_patch.cpp.patch`

How to use
==========
* Create 3 files: the old/source file to diff from, the new/target file to diff to, and the patch file to apply the patches to.
* Define a 4th file where to write the output to. This can be the same as the 3rd file, patching it in place.
* call the tool e.g.: `diffmorpher -s source.txt -t target.txt -p patch.txt -o out.txt -a -f`

```
usage: diffmorpher
   options:
      -s, --source     Source file for diffs
      -t, --target     Target file for diffs
      -p, --patch      File to apply patches to
      -o, --out        Output file
      -a, --auto       auto delete/create files which are non existing on either side
      -d, --dirs       handle as directories (ignore hidden folders)
      -f, --force      force patching different file length (truncate or pad with space)
```
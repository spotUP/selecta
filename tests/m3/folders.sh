#!/bin/sh
# T83 DB half driver (I178): folder tree commands, cycle and non-empty rules, restart, folder export. Prints folders_ok.
# usage: folders.sh <selecta-cli> <catalogue.tsv> <folders.sel> <folders2.sel> <tree.golden> <export.golden>
set -u
cli=$1
rm -rf folders.db folders_export
out1=$("$cli" script "$3" --catalogue "$2" --db folders.db 2>&1) || { echo "$out1"; exit 1; }
for want in 'ERROR folder_cycle FOLDER_MOVE' 'ERROR folder_not_empty FOLDER_DEL' 'ERROR no_such_folder FOLDER_DEL' \
            'ERROR no_such_playlist PLAYLIST_MOVE' 'folders=3 playlists_in_folders=3 playlists=3' 'folder_ops=17'; do
    printf '%s\n' "$out1" | grep -q "$want" || { echo "missing '$want':"; printf '%s\n' "$out1"; exit 1; }
done
cycles=$(printf '%s\n' "$out1" | grep -c 'ERROR folder_cycle')
[ "$cycles" = 2 ] || { echo "folder_cycle answered $cycles times, want 2"; exit 1; }
printf '%s\n' "$out1" | grep -A100 '^F 1 ' > folders_tree.out
diff folders_tree.out "$5" || { echo "tree differs from golden (first process)"; exit 1; }
out2=$("$cli" script "$4" --catalogue "$2" --db folders.db 2>&1) || { echo "$out2"; exit 1; }
printf '%s\n' "$out2" | grep -q 'folders=3 playlists_in_folders=3 playlists=3 entries=6 folder_ops=0' || { echo "counts lost over restart:"; printf '%s\n' "$out2"; exit 1; }
printf '%s\n' "$out2" | grep -A100 '^F 1 ' > folders_tree.out
diff folders_tree.out "$5" || { echo "tree differs from golden after restart"; exit 1; }
exp=$("$cli" library export-folder --db folders.db --dir folders_export --folder 1) || { echo "$exp"; exit 1; }
[ "$exp" = "exported=3 folder=1" ] || { echo "export said: $exp"; exit 1; }
files=$(ls folders_export | wc -l | tr -d ' ')
[ "$files" = 3 ] || { echo "export wrote $files files, want 3"; exit 1; }
cat folders_export/*.m3u > folders_export.out
diff folders_export.out "$6" || { echo "export differs from golden"; exit 1; }
sub=$("$cli" library export-folder --db folders.db --dir folders_export2 --folder 2) || exit 1
[ "$sub" = "exported=1 folder=2" ] || { echo "sub-folder export said: $sub"; exit 1; }
rm -rf folders.db folders_export folders_export2 folders_tree.out folders_export.out
echo folders_ok

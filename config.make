# Note: indention matters, use TAB below
cat <<EOF >>$NGX_MAKEFILE

$ngx_addon_dir/lmdb/libraries/liblmdb/liblmdb.a:
	echo "Building liblmdb"; \\
	\$(MAKE) -C $ngx_addon_dir/lmdb/libraries/liblmdb -f $ngx_addon_dir/Makefile.lmdb liblmdb.a; \\
	echo "Finished building liblmdb"

$ngx_addon_dir/lmdb/libraries/liblmdb/chacha8.a:
	echo "Building chacha8"; \\
	\$(MAKE) -C $ngx_addon_dir/lmdb/libraries/liblmdb -f $ngx_addon_dir/Makefile.lmdb chacha8.a; \\
	echo "Finished building chacha8"

EOF
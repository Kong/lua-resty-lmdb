# Note: indention matters, use TAB below
cat <<EOF >>$NGX_MAKEFILE

$ngx_addon_dir/lmdb/libraries/liblmdb/liblmdb.a:
	echo "Building liblmdb"; \\
	\$(MAKE) -C $ngx_addon_dir/lmdb/libraries/liblmdb -f $ngx_addon_dir/Makefile.lmdb liblmdb.a; \\
	echo "Finished building liblmdb"

EOF
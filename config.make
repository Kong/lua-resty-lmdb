# Note: indention matters, use TAB below
cat <<EOF >>$NGX_MAKEFILE

$ngx_addon_dir/lmdb/libraries/liblmdb/liblmdb.a:
	echo "Building liblmdb"; \\
	\$(MAKE) XCFLAGS="-fPIC" -C $ngx_addon_dir/lmdb/libraries/liblmdb; \\
	echo "Finished building liblmdb"

EOF

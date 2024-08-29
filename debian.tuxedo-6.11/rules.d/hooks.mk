do_libc_dev_package	= false
do_doc_package		= false
do_tools_common		= false
do_tools_host		= false
do_lib_rust		= false

# gcc-12 is the newest gcc available for jammy. This overwrites the gcc-14
# default.
gcc			= gcc-12

# rtls requires newer versions of libtraceevent-dev and libtracefs-dev then
# available in jammy since kernel 6.9.
do_tools_rtla		= false
